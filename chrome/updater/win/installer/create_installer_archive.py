# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to create the Chrome Updater Installer archive.

  This script is used to create an archive of all the files required for a
  Chrome Updater install in appropriate directory structure. It reads
  updater.release file as input, creates updater.7z ucompressed archive, and
  generates the updater.packed.7z compressed archive.

"""

import configparser
import glob
import optparse
import os
import shutil
import subprocess
import sys

# Directory name inside the uncompressed archive where all the files are.
UPDATER_DIR = "bin"

# Suffix to uncompressed full archive file, appended to options.output_name.
ARCHIVE_SUFFIX = ".7z"

# compressed full archive suffix, will be prefixed by options.output_name.
COMPRESSED_ARCHIVE_SUFFIX = ".packed.7z"
TEMP_ARCHIVE_DIR = "temp_installer_archive"

g_archive_inputs = []


def CompressUsingLZMA(build_dir, compressed_file, input_file, verbose, fast):
    lzma_exec = GetLZMAExec(build_dir)
    cmd = [
        lzma_exec,
        'a',
        '-t7z',
    ]
    if fast:
        cmd.append('-mx1')
    else:
        cmd.extend([
            # Flags equivalent to -mx9 (ultra) but with the bcj2 turned on (exe
            # pre-filter). These arguments are the similar to what the Chrome
            # mini-installer is using.
            '-m0=BCJ2',
            '-m1=LZMA:d27:fb128',
            '-m2=LZMA:d22:fb128:mf=bt2',
            '-m3=LZMA:d22:fb128:mf=bt2',
            '-mb0:1',
            '-mb0s1:2',
            '-mb0s2:3',
        ])
    cmd.extend([
        os.path.abspath(compressed_file),
        os.path.abspath(input_file),
    ])
    if os.path.exists(compressed_file):
        os.remove(compressed_file)
    RunSystemCommand(cmd, verbose)


def CopyAllFilesToStagingDir(config, staging_dir, build_dir, timestamp,
                             include_enterprise_companion):
    """Copies the files required for installer archive."""
    CopySectionFilesToStagingDir(config, 'GENERAL', staging_dir, build_dir,
                                 timestamp)
    if include_enterprise_companion:
        CopySectionFilesToStagingDir(config, 'ENTERPRISE_COMPANION',
                                     staging_dir, build_dir, timestamp)


def CopySectionFilesToStagingDir(config, section, staging_dir, src_dir,
                                 timestamp):
    """Copies installer archive files specified in section from src_dir to
    staging_dir. This method reads section from config and copies all the
    files specified from src_dir to staging dir."""
    for option in config.options(section):
        src_subdir = option.replace('\\', os.sep)
        dst_dir = os.path.join(staging_dir, config.get(section, option))
        dst_dir = dst_dir.replace('\\', os.sep)
        src_paths = glob.glob(os.path.join(src_dir, src_subdir))
        for src_path in src_paths:
            if dst_dir.endswith(os.sep):
                dst_path = os.path.join(dst_dir, os.path.basename(src_path))
            else:
                dst_path = dst_dir
            if not os.path.exists(dst_path):
                if not os.path.exists(os.path.dirname(dst_path)):
                    os.makedirs(os.path.dirname(dst_dir))
                g_archive_inputs.append(src_path)
                shutil.copy(src_path, dst_path)
                os.utime(dst_path, (os.stat(dst_path).st_atime, timestamp))
        os.utime(dst_dir, (os.stat(dst_dir).st_atime, timestamp))


def GetLZMAExec(build_dir):
    executable = '7za'
    if sys.platform == 'win32':
        executable += '.exe'

    return os.path.join(build_dir, "..", "..", "third_party", "lzma_sdk",
                        "bin", "host_platform", executable)


def MakeStagingDirectory(staging_dir):
    """Creates a staging path for installer archive. If directory exists
    already, deletes the existing directory."""
    file_path = os.path.join(staging_dir, TEMP_ARCHIVE_DIR)
    if os.path.exists(file_path):
        shutil.rmtree(file_path)
    os.makedirs(file_path)
    return file_path


def Readconfig(input_file):
    """Reads config information from input file after setting default value of
    global variables."""
    variables = {}
    variables['UpdaterDir'] = UPDATER_DIR
    config = configparser.ConfigParser(variables)
    config.read(input_file)
    return config


def RunSystemCommand(cmd, verbose):
    """Runs |cmd|, prints the |cmd| and its output if |verbose|; otherwise
    captures its output and only emits it on failure."""
    if verbose:
        print('Running', cmd)

    try:
        # Run |cmd|, redirecting stderr to stdout in order for captured errors
        # to be inline with corresponding stdout.
        output = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
        if verbose:
            print(output)
    except subprocess.CalledProcessError as e:
        raise Exception("Error while running cmd: %s\n"
                        "Exit code: %s\n"
                        "Command output:\n%s" %
                        (e.cmd, e.returncode, e.output))


def CreateArchiveFile(options, staging_dir, timestamp):
    """Creates a new installer archive file after deleting any existing old
    file."""
    # First create an uncompressed archive file for the current build
    # (updater.7z)
    lzma_exec = GetLZMAExec(options.build_dir)
    archive_file = os.path.join(options.output_dir,
                                options.output_name + ARCHIVE_SUFFIX)

    if options.depfile:
        # If a depfile was requested, do the glob of the staging dir and
        # generate a list of dependencies in .d format. We list the files that
        # were copied into the staging dir, not the files that are actually in
        # the staging dir because the ones in the staging dir will never be
        # edited, and we want to have the build be triggered when the
        # thing-that-was-copied-there changes.

        def PathFixup(path):
            """Fixes path for depfile format: backslash to forward slash, and
      backslash escaping for spaces."""
            return path.replace('\\', '/').replace(' ', '\\ ')

        # Gather the list of files in the staging dir that will be zipped up.
        # We only gather this list to make sure that g_archive_inputs is
        # complete (i.e. that there's not file copies that got missed).
        staging_contents = []
        for root, files in os.walk(os.path.join(staging_dir, UPDATER_DIR)):
            for filename in files:
                staging_contents.append(PathFixup(os.path.join(root,
                                                               filename)))

        # Make sure there's an archive_input for each staging dir file.
        for staging_file in staging_contents:
            for archive_input in g_archive_inputs:
                archive_rel = PathFixup(archive_input)
                if (os.path.basename(staging_file).lower() == os.path.basename(
                        archive_rel).lower()):
                    break
            else:
                raise Exception('Did not find an archive input file for "%s"' %
                                staging_file)

        # Finally, write the depfile referencing the inputs.
        with open(options.depfile, 'wb') as f:
            f.write(
                PathFixup(os.path.relpath(archive_file, options.build_dir)) +
                ': \\\n')
            f.write('  ' +
                    ' \\\n  '.join(PathFixup(x) for x in g_archive_inputs))

    # It is important to use abspath to create the path to the directory because
    # if you use a relative path without any .. sequences then 7za.exe uses the
    # entire relative path as part of the file paths in the archive. If you have
    # a .. sequence or an absolute path then only the last directory is stored
    # as part of the file paths in the archive, which is what we want.
    cmd = [
        lzma_exec,
        'a',
        '-t7z',
        archive_file,
        os.path.abspath(os.path.join(staging_dir, UPDATER_DIR)),
        '-mx0',
    ]
    # There does not seem to be any way in 7za.exe to override existing file so
    # we always delete before creating a new one.
    if not os.path.exists(archive_file):
        RunSystemCommand(cmd, options.verbose)
    elif options.skip_rebuild_archive != "true":
        os.remove(archive_file)
        RunSystemCommand(cmd, options.verbose)

    compressed_archive_file = options.output_name + COMPRESSED_ARCHIVE_SUFFIX
    compressed_archive_file_path = os.path.join(options.output_dir,
                                                compressed_archive_file)
    os.utime(archive_file, (os.stat(archive_file).st_atime, timestamp))
    CompressUsingLZMA(options.build_dir, compressed_archive_file_path,
                      archive_file, options.verbose,
                      options.fast_archive_compression)

    return compressed_archive_file


_RESOURCE_FILE_HEADER = """\
// This file is automatically generated by create_installer_archive.py.
// It contains the resource entries that are going to be linked inside the exe.
// For each file to be linked there should be two lines:
// - The first line contains the output filename (without path) and the
// type of the resource ('BN' - not compressed , 'BL' - LZ compressed,
// 'B7' - LZMA compressed)
// - The second line contains the path to the input file. Uses '/' to
// separate path components.
"""


def CreateResourceInputFile(output_dir, archive_file, resource_file_path,
                            component_build, staging_dir):
    """Creates resource input file for installer target."""

    # An array of (file, type, path) tuples of the files to be included.
    resources = [(archive_file, 'B7', os.path.join(output_dir, archive_file))]

    with open(resource_file_path, 'w') as f:
        f.write(_RESOURCE_FILE_HEADER)
        for (file, type, path) in resources:
            f.write('\n%s  %s\n    "%s"\n' %
                    (file, type, path.replace("\\", "/")))


def ParseDLLsFromDeps(build_dir, runtime_deps_file):
    """Parses the runtime_deps file and returns the set of DLLs in it, relative
    to build_dir."""
    build_dlls = set()
    args = open(runtime_deps_file).read()
    for l in args.splitlines():
        if os.path.splitext(l)[1] == ".dll":
            build_dlls.add(os.path.join(build_dir, l))
    return build_dlls


# Copies component build DLLs for the setup to be able to find those DLLs at
# run-time.
# This is meant for developer builds only and should never be used to package
# an official build.
def DoComponentBuildTasks(staging_dir, build_dir, setup_runtime_deps):
    installer_dir = os.path.join(staging_dir, UPDATER_DIR)
    if not os.path.exists(installer_dir):
        os.mkdir(installer_dir)

    setup_component_dlls = ParseDLLsFromDeps(build_dir, setup_runtime_deps)

    for setup_component_dll in setup_component_dlls:
        g_archive_inputs.append(setup_component_dll)
        shutil.copy(setup_component_dll, installer_dir)


def main(options):
    """Main method that reads input file, creates archive file and writes
    resource input file."""
    config = Readconfig(options.input_file)

    staging_dir = MakeStagingDirectory(options.staging_dir)

    # Copy the files from the build dir.
    CopyAllFilesToStagingDir(config, staging_dir, options.build_dir,
                             options.timestamp,
                             options.include_enterprise_companion)

    if options.component_build == '1':
        DoComponentBuildTasks(staging_dir, options.build_dir,
                              options.setup_runtime_deps)

    # Name of the archive file built (for example - updater.7z)
    archive_file = CreateArchiveFile(options, staging_dir, options.timestamp)
    CreateResourceInputFile(options.output_dir, archive_file,
                            options.resource_file_path,
                            options.component_build == '1', staging_dir)


def _ParseOptions():
    parser = optparse.OptionParser()
    parser.add_option('-i',
                      '--input_file',
                      help='Input file describing which files to archive.')
    parser.add_option(
        '-b',
        '--build_dir',
        help='Build directory. The paths in input_file are relative to this.')
    parser.add_option(
        '--staging_dir',
        help='Staging directory where intermediate files and directories '
        'will be created')
    parser.add_option(
        '-o',
        '--output_dir',
        help='The output directory where the archives will be written. '
        'Defaults to the build_dir.')
    parser.add_option('--resource_file_path',
                      help='The path where the resource file will be output. ')
    parser.add_option('-s',
                      '--skip_rebuild_archive',
                      default="False",
                      help='Skip re-building updater.7z archive if it exists.')
    parser.add_option('-n',
                      '--output_name',
                      default='updater',
                      help='Name used to prefix names of generated archives.')
    parser.add_option(
        '--component_build',
        default='0',
        help='Whether this archive is packaging a component build.')
    parser.add_option(
        '--fast_archive_compression',
        action='store_true',
        default=False,
        help='Enable fast compression of updater.7z into updater.packed.7z and '
        'helpfully delete any old updater.packed.7z in |output_dir|.')
    parser.add_option(
        '--depfile',
        help=
        'Generate a depfile with the given name listing the implicit inputs '
        'to the archive process that can be used with a build system.')
    parser.add_option(
        '--setup_runtime_deps',
        help='A file listing runtime dependencies for setup.exe. This will be '
        'used to get a list of DLLs to archive in a component build.')
    parser.add_option('-v',
                      '--verbose',
                      action='store_true',
                      dest='verbose',
                      default=False)
    parser.add_option('--timestamp',
                      type='int',
                      help='Timestamp to set archive entry modified times to.')
    parser.add_option(
        '--include_enterprise_companion',
        action='store_true',
        dest='include_enterprise_companion',
        default=False,
        help=
        'Whether the Chrome Enterprise Companion App should be included in the '
        'archive.')

    options, _ = parser.parse_args()
    if not options.build_dir:
        parser.error('You must provide a build dir.')

    options.build_dir = os.path.normpath(options.build_dir)

    if not options.staging_dir:
        parser.error('You must provide a staging dir.')

    if not options.input_file:
        parser.error('You must provide an input file')

    is_component_build = options.component_build == '1'
    if is_component_build and not options.setup_runtime_deps:
        parser.error(
            "updater_runtime_deps must be specified for a component build")

    if not options.output_dir:
        options.output_dir = options.build_dir

    return options


if '__main__' == __name__:
    options = _ParseOptions()
    if options.verbose:
        print(sys.argv)
    sys.exit(main(options))
