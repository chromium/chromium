#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to create Chrome Installer archive.

  This script is used to create an archive of all the files required for a
  Chrome install in appropriate directory structure. It reads chrome.release
  file as input, creates chrome.7z archive, compresses setup.exe and
  generates packed_files.txt for mini_installer project.

"""

import configparser
import fnmatch
import glob
import optparse
import os
import shutil
import subprocess
import sys

ARCHIVE_DIR = "installer_archive"

# suffix to uncompressed full archive file, appended to options.output_name
ARCHIVE_SUFFIX = ".7z"
BSDIFF_EXEC = "bsdiff.exe"
CHROME_DIR = "Chrome-bin"
CHROME_PATCH_FILE_SUFFIX = "_patch"  # prefixed by options.output_name

# compressed full archive suffix, will be prefixed by options.output_name
COMPRESSED_ARCHIVE_SUFFIX = ".packed.7z"

COMPRESSED_FILE_EXT = ".packed.7z"  # extension of patch archive file
ZUCCHINI_EXEC = "zucchini.exe"
MINI_INSTALLER_INPUT_FILE = "packed_files.txt"
PATCH_FILE_EXT = '.diff'
SETUP_EXEC = "setup.exe"
SETUP_PATCH_FILE_PREFIX = "setup_patch"
TEMP_ARCHIVE_DIR = "temp_installer_archive"
VERSION_FILE = "VERSION"

g_archive_inputs = []


def BuildVersion():
    """Returns the full build version string constructed from information in
    VERSION_FILE.  Any segment not found in that file will default to '0'.
    """
    major = 0
    minor = 0
    build = 0
    patch = 0
    # The version file is located at the directory ${CHROMIUM_SRC}/chrome.
    version_file_path = os.path.join(os.path.dirname(__file__), '../../..',
                                     VERSION_FILE)
    for line in open(version_file_path, 'r'):
        line = line.rstrip()
        if line.startswith('MAJOR='):
            major = line[6:]
        elif line.startswith('MINOR='):
            minor = line[6:]
        elif line.startswith('BUILD='):
            build = line[6:]
        elif line.startswith('PATCH='):
            patch = line[6:]
    return '%s.%s.%s.%s' % (major, minor, build, patch)


def CompressUsingLZMA(build_dir,
                      compressed_file,
                      input_file,
                      verbose,
                      fast,
                      strip_time=False):
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
            # pre-filter). This results in a ~2.3MB decrease in installer size
            # on a 24MB installer.
            # Additionally, these settings reflect a 7zip 4.42 and up change in
            # the definition of -mx9, increasting the dictionary size moving to
            # 26bit = 64MB. This results in an additional ~3.5MB decrease.
            # Older 7zip versions can support these settings, as these changes
            # rely on existing functionality in the lzma format.
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
    # 'tm' means to store last modified timestamps for files in the
    # archive(default on). If strip_time is true, attach '-' to turn it off.
    # See https://sourceforge.net/p/sevenzip/discussion/45797/thread/61905a4c.
    if strip_time:
        cmd.append('-mtm-')
    if os.path.exists(compressed_file):
        os.remove(compressed_file)
    RunSystemCommand(cmd, verbose)


def OverwriteStagingBuildTime(timestamp_str):
    """Overwrite created and modified time for all entities in staging folder.

    Args:
      * timestamp_str (str): Epoch second.
    """
    timestamp = int(timestamp_str)
    abs_stg = os.path.abspath(options.staging_dir)
    for root, dirs, files in os.walk(abs_stg, topdown=False):
        [
            os.utime(os.path.join(abs_stg, root, x), (timestamp, timestamp))
            for x in dirs + files
        ]


def CopyAllFilesToStagingDir(config, distribution, staging_dir, build_dir,
                             enable_hidpi, include_snapshotblob, include_dxc,
                             component_build, component_ffmpeg_build, verbose):
    """Copies the files required for installer archive.
    Copies all common files required for various distributions of Chromium and
    also files for the specific Chromium build specified by distribution.
    """
    CopySectionFilesToStagingDir(config, 'GENERAL', staging_dir, build_dir,
                                 verbose)
    if distribution:
        if len(distribution) > 1 and distribution[0] == '_':
            distribution = distribution[1:]

        distribution = distribution.upper()
        if config.has_section(distribution):
            CopySectionFilesToStagingDir(config, distribution, staging_dir,
                                         build_dir, verbose)
    if enable_hidpi == '1':
        CopySectionFilesToStagingDir(config, 'HIDPI', staging_dir, build_dir,
                                     verbose)

    if include_snapshotblob == '1':
        CopySectionFilesToStagingDir(config, 'SNAPSHOTBLOB', staging_dir,
                                     build_dir, verbose)

    if include_dxc == '1':
        CopySectionFilesToStagingDir(config, 'DXC', staging_dir, build_dir,
                                     verbose)

    if component_build != '1' and component_ffmpeg_build == '1':
        CopySectionFilesToStagingDir(config, 'FFMPEG', staging_dir, build_dir,
                                     verbose)

    # If build_time specified, overwrite it to every object in the
    # staging folder, making the compressed package deterministic.
    if options.build_time:
        OverwriteStagingBuildTime(options.build_time)


# The 'ConfigParser' makes all strings lowercase - which works fine on
# a cases-insensitive NTFS partition, but makes no sense when trying to build
# mini_installer.exe on a linux box. This function can be used to make glob
# matches case insensitive to bypass this issue.
def insensiglob(pattern):

    def recase(c):
        return '[{}{}]'.format(c.lower(), c.upper()) if c.isalpha() else c

    return glob.glob(''.join(map(recase, pattern)))


def CopySectionFilesToStagingDir(config, section, staging_dir, src_dir,
                                 verbose):
    """Copies installer archive files specified in section from src_dir to
    staging_dir. This method reads section from config and copies all the
    files specified from src_dir to staging dir.
    """
    for option in config.options(section):
        if option.endswith('dir'):
            continue

        src_subdir = option.replace('\\', os.sep)
        dst_dir = os.path.join(staging_dir, config.get(section, option))
        dst_dir = dst_dir.replace('\\', os.sep)
        # There are specific issues with libEGL.dll and libGLESv2.dll which
        # require insensitive globbing on linux machines.
        src_paths = insensiglob(os.path.join(src_dir, src_subdir))
        if verbose and not src_paths:
            print('No matches found for {} in {}'.format(
                option, os.path.join(os.getcwd(), src_dir)))
        if src_paths and not os.path.exists(dst_dir):
            os.makedirs(dst_dir)
        for src_path in src_paths:
            dst_path = os.path.join(dst_dir, os.path.basename(src_path))
            if not os.path.exists(dst_path):
                g_archive_inputs.append(src_path)
                shutil.copy(src_path, dst_dir)


def GenerateDiffPatch(options, orig_file, new_file, patch_file):
    if (options.diff_algorithm == "ZUCCHINI"):
        exe_file = os.path.join(options.last_chrome_installer, ZUCCHINI_EXEC)
        cmd = '%s -gen "%s" "%s" "%s"' % (exe_file, orig_file, new_file,
                                          patch_file)
    else:
        exe_file = os.path.join(options.build_dir, BSDIFF_EXEC)
        cmd = [
            exe_file,
            orig_file,
            new_file,
            patch_file,
        ]
    RunSystemCommand(cmd, options.verbose)


def GetLZMAExec(build_dir):
    executable = '7za'
    if sys.platform == 'win32':
        executable += '.exe'

    return os.path.join(build_dir, "..", "..", "third_party", "lzma_sdk", "bin",
                        "host_platform", executable)


def GetPrevVersion(build_dir, temp_dir, last_chrome_installer, output_name):
    if not last_chrome_installer:
        return ''

    lzma_exec = GetLZMAExec(build_dir)
    prev_archive_file = os.path.join(last_chrome_installer,
                                     output_name + ARCHIVE_SUFFIX)
    cmd = [
        lzma_exec,
        'x',
        '-o"%s"' % temp_dir,
        prev_archive_file,
        'Chrome-bin/*/chrome.dll',
    ]
    RunSystemCommand(cmd, options.verbose)
    dll_path = glob.glob(os.path.join(temp_dir, 'Chrome-bin', '*',
                                      'chrome.dll'))
    return os.path.split(os.path.split(dll_path[0])[0])[1]


def MakeStagingDirectory(staging_dir):
    """Creates a staging path for installer archive. If directory exists
    already, deletes the existing directory.
    """
    file_path = os.path.join(staging_dir, TEMP_ARCHIVE_DIR)
    if os.path.exists(file_path):
        shutil.rmtree(file_path)
    os.makedirs(file_path)
    return file_path


def Readconfig(input_file, current_version):
    """Reads config information from input file after setting default value of
    global variables.
    """
    variables = {}
    variables['ChromeDir'] = CHROME_DIR
    variables['VersionDir'] = os.path.join(variables['ChromeDir'],
                                           current_version)
    config = configparser.ConfigParser(variables)
    config.read(input_file)
    return config


def RunSystemCommand(cmd, verbose):
    """Runs |cmd|, prints the |cmd| and its output if |verbose|; otherwise
    captures its output and only emits it on failure.
    """
    if verbose:
        print('Running %s' % ' '.join(cmd))

    try:
        # Run |cmd|, redirecting stderr to stdout in order for captured errors
        # to be inline with corresponding stdout.
        output = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
        if verbose:
            print(output)
    except subprocess.CalledProcessError as e:
        raise Exception("Error while running cmd: %s\n"
                        "Exit code: %s\n"
                        "Command output:\n%s" % (e.cmd, e.returncode, e.output))


def CreateArchiveFiles(options, staging_dir, current_version, prev_version):
    """Creates a new installer archive file after deleting any existing old
    file.
    Returns:
        A tuple of the names of the compressed and uncompressed archive files,
        in that order.
    """
    # First create an uncompressed archive file for the current build
    # (chrome.7z)
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

        def path_fixup(path):
            """Fixes path for depfile format: backslash to forward slash, and
            backslash escaping for spaces."""
            return path.replace('\\', '/').replace(' ', '\\ ')

        # Gather the list of files in the staging dir that will be zipped up. We
        # only gather this list to make sure that g_archive_inputs is complete
        # (i.e. that there's not file copies that got missed).
        staging_contents = []
        for root, dirs, files in os.walk(os.path.join(staging_dir, CHROME_DIR)):
            for filename in files:
                staging_contents.append(path_fixup(os.path.join(root,
                                                                filename)))

        # Make sure there's an archive_input for each staging dir file.
        for staging_file in staging_contents:
            for archive_input in g_archive_inputs:
                archive_rel = path_fixup(archive_input)
                if (os.path.basename(staging_file).lower() == os.path.basename(
                        archive_rel).lower()):
                    break
            else:
                raise Exception('Did not find an archive input file for "%s"' %
                                staging_file)

        # Finally, write the depfile referencing the inputs.
        with open(options.depfile, 'wb') as f:
            f.write(
                (path_fixup(os.path.relpath(archive_file, options.build_dir)) +
                 ': \\\n').encode())
            f.write(
                ('  ' + ' \\\n  '.join(path_fixup(x)
                                       for x in g_archive_inputs)).encode())

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
        os.path.abspath(os.path.join(staging_dir, CHROME_DIR)),
        '-mx0',
    ]
    # There doesnt seem to be any way in 7za.exe to override existing file so
    # we always delete before creating a new one.
    if not os.path.exists(archive_file):
        RunSystemCommand(cmd, options.verbose)
    elif options.skip_rebuild_archive != "true":
        os.remove(archive_file)
        RunSystemCommand(cmd, options.verbose)

    # If we are generating a patch, run bsdiff against previous build and
    # compress the resulting patch file. If this is not a patch just compress
    # the uncompressed archive file.
    patch_name_prefix = options.output_name + CHROME_PATCH_FILE_SUFFIX
    if options.last_chrome_installer:
        prev_archive_file = os.path.join(options.last_chrome_installer,
                                         options.output_name + ARCHIVE_SUFFIX)
        patch_file = os.path.join(options.build_dir,
                                  patch_name_prefix + PATCH_FILE_EXT)
        GenerateDiffPatch(options, prev_archive_file, archive_file, patch_file)
        compressed_archive_file = patch_name_prefix + '_' + \
                                  current_version + '_from_' + prev_version + \
                                  COMPRESSED_FILE_EXT
        orig_file = patch_file
    else:
        compressed_archive_file = (options.output_name +
                                   COMPRESSED_ARCHIVE_SUFFIX)
        orig_file = archive_file

    compressed_archive_file_path = os.path.join(options.output_dir,
                                                compressed_archive_file)
    CompressUsingLZMA(
        options.build_dir,
        compressed_archive_file_path,
        orig_file,
        options.verbose,
        options.fast_archive_compression,
        # If build time is specified, the compressed artifact
        # should be deterministic. So in the archive, aka
        # chrome.packed.7z, strip chrome.7z's timestamp.
        strip_time=True)

    return compressed_archive_file, os.path.basename(archive_file)


def PrepareSetupExec(options, current_version, prev_version):
    """Prepares setup.exe for bundling in mini_installer based on options."""
    if options.setup_exe_format == 'FULL':
        setup_file = SETUP_EXEC
    elif options.setup_exe_format == 'DIFF':
        if not options.last_chrome_installer:
            raise Exception(
                "To use DIFF for setup.exe, --last_chrome_installer is needed.")
        prev_setup_file = os.path.join(options.last_chrome_installer,
                                       SETUP_EXEC)
        new_setup_file = os.path.join(options.build_dir, SETUP_EXEC)
        patch_file = os.path.join(options.build_dir,
                                  SETUP_PATCH_FILE_PREFIX + PATCH_FILE_EXT)
        GenerateDiffPatch(options, prev_setup_file, new_setup_file, patch_file)
        setup_file = SETUP_PATCH_FILE_PREFIX + '_' + current_version + \
                    '_from_' + prev_version + COMPRESSED_FILE_EXT
        setup_file_path = os.path.join(options.build_dir, setup_file)
        CompressUsingLZMA(options.build_dir, setup_file_path, patch_file,
                          options.verbose, options.fast_archive_compression)
    else:
        # Use makecab.py instead of makecab.exe so that this works when building
        # on non-Windows hosts too.
        makecab_py = os.path.join(os.path.dirname(__file__), 'makecab.py')
        cmd = [
            sys.executable,
            makecab_py,
            '/D',
            'CompressionType=LZX',
            '/V1',
            '/L',
            options.output_dir,
            os.path.join(options.build_dir, SETUP_EXEC),
        ]
        RunSystemCommand(cmd, options.verbose)
        setup_file = SETUP_EXEC[:-1] + "_"
    return setup_file


_RESOURCE_FILE_HEADER = """\
// This file is automatically generated by create_installer_archive.py.
// It contains the resource entries that are going to be linked inside
// mini_installer.exe. For each file to be linked there should be two
// lines:
// - The first line contains the output filename (without path) and the
// type of the resource:
//
// 'BN' - not compressed binary,
// 'BD' - uncompressed dependency for component builds,
// 'BL' - LZ compressed,
// 'B7' - LZMA compressed)
//
// - The second line contains the path to the input file. Uses '/' to
// separate path components.
"""


def CreateResourceInputFile(output_dir, setup_file, setup_resource_type,
                            archive_file, archive_resource_type,
                            resource_file_path, component_build, staging_dir,
                            current_version):
    """Creates resource input file (packed_files.txt) for mini_installer
    project.
    """
    # An array of (file, type, path) tuples of the files to be included.
    resources = [(setup_file, setup_resource_type,
                  os.path.join(output_dir, setup_file)),
                 (archive_file, archive_resource_type,
                  os.path.join(output_dir, archive_file))]

    # Include all files needed to run setup.exe (these are copied into the
    # 'Installer' dir by DoComponentBuildTasks).
    if component_build:
        installer_dir = os.path.join(staging_dir, CHROME_DIR, current_version,
                                     'Installer')
        resources += [(file, 'BD', os.path.join(installer_dir, file))
                      for file in os.listdir(installer_dir)]

    with open(resource_file_path, 'w') as f:
        f.write(_RESOURCE_FILE_HEADER)
        for (file, type, path) in resources:
            f.write('\n%s  %s\n    "%s"\n' %
                    (file, type, path.replace("\\", "/")))


# Reads |manifest_name| from |build_dir| and writes |manifest_name| to
# |output_dir| with the same content plus |inserted_string| added just before
# |insert_before|.
def CopyAndAugmentManifest(build_dir, output_dir, manifest_name,
                           inserted_string, insert_before):
    with open(os.path.join(build_dir, manifest_name), 'r') as f:
        manifest_lines = f.readlines()

    insert_line = -1
    insert_pos = -1
    for i in range(len(manifest_lines)):
        insert_pos = manifest_lines[i].find(insert_before)
        if insert_pos != -1:
            insert_line = i
            break
    if insert_line == -1:
        raise ValueError('Could not find {0} in the manifest:\n{1}'.format(
            insert_before, ''.join(manifest_lines)))
    old = manifest_lines[insert_line]
    manifest_lines[insert_line] = (old[:insert_pos] + '\n' + inserted_string +
                                   '\n' + old[insert_pos:])

    with open(os.path.join(output_dir, manifest_name), 'w') as f:
        f.write(''.join(manifest_lines))


def CopyIfChanged(src, target_dir):
    """Copy specified |src| file to |target_dir|, but only write to target if
    the file has changed. This avoids a problem during packaging where parts of
    the build have not completed and have the runtime DLL locked when we try to
    copy over it. See http://crbug.com/305877 for details.
    """
    assert os.path.isdir(target_dir)
    dest = os.path.join(target_dir, os.path.basename(src))
    g_archive_inputs.append(src)
    if os.path.exists(dest):
        # We assume the files are OK to buffer fully into memory since we know
        # they're only 1-2M.
        with open(src, 'rb') as fsrc:
            src_data = fsrc.read()
        with open(dest, 'rb') as fdest:
            dest_data = fdest.read()
        if src_data != dest_data:
            # This may still raise if we get here, but this really should almost
            # never happen (it would mean that the contents of
            # e.g. msvcr100d.dll had been changed).
            shutil.copyfile(src, dest)
    else:
        shutil.copyfile(src, dest)


def ParseDLLsFromDeps(build_dir, runtime_deps_file):
    """Parses the runtime_deps file and returns the set of DLLs in it, relative
    to build_dir.
    """
    build_dlls = set()
    args = open(runtime_deps_file).read()
    for l in args.splitlines():
        if os.path.splitext(l)[1] == ".dll":
            build_dlls.add(os.path.join(build_dir, l))
    return build_dlls


# Copies component build DLLs and generates required config files and manifests
# in order for chrome.exe and setup.exe to be able to find those DLLs at
# run-time.
# This is meant for developer builds only and should never be used to package
# an official build.
def DoComponentBuildTasks(staging_dir, build_dir, target_arch,
                          setup_runtime_deps, chrome_runtime_deps,
                          current_version):
    # Get the required directories for the upcoming operations.
    chrome_dir = os.path.join(staging_dir, CHROME_DIR)
    version_dir = os.path.join(chrome_dir, current_version)
    installer_dir = os.path.join(version_dir, 'Installer')
    # |installer_dir| is technically only created post-install, but we need it
    # now to add setup.exe's config and manifest to the archive.
    if not os.path.exists(installer_dir):
        os.mkdir(installer_dir)

    setup_component_dlls = ParseDLLsFromDeps(build_dir, setup_runtime_deps)

    for setup_component_dll in setup_component_dlls:
        g_archive_inputs.append(setup_component_dll)
        shutil.copy(setup_component_dll, installer_dir)

    # Stage all the component DLLs to the |version_dir| (for
    # the version assembly to be able to refer to them below and make sure
    # chrome.exe can find them at runtime), except the ones that are already
    # staged (i.e. non-component DLLs).
    build_dlls = ParseDLLsFromDeps(build_dir, chrome_runtime_deps)
    # Generate a list of relative dll paths that have already been staged into
    # the version directory (i.e., non-component DLLs).
    staged_dlls = [os.path.normcase(os.path.relpath(os.path.join(dir, file),
                                                    version_dir)) \
                   for dir, _, files in os.walk(version_dir) \
                   for file in files if fnmatch.fnmatch(file, '*.dll')]
    component_dll_filenames = []
    for component_dll in [dll for dll in build_dlls if \
                          os.path.normcase(os.path.relpath(dll, build_dir)) \
                          not in staged_dlls]:
        component_dll_name = os.path.basename(component_dll)
        component_dll_filenames.append(component_dll_name)
        g_archive_inputs.append(component_dll)
        shutil.copy(component_dll, version_dir)

    # Augment {version}.manifest to include all component DLLs as part of the
    # assembly it constitutes, which will allow dependents of this assembly to
    # find these DLLs.
    version_assembly_dll_additions = []
    for dll_filename in component_dll_filenames:
        version_assembly_dll_additions.append("  <file name='%s'/>" %
                                              dll_filename)
    CopyAndAugmentManifest(build_dir, version_dir,
                           '%s.manifest' % current_version,
                           '\n'.join(version_assembly_dll_additions),
                           '</assembly>')


def main(options):
    """Main method that reads input file, creates archive file and writes
    resource input file.
    """
    current_version = BuildVersion()

    config = Readconfig(options.input_file, current_version)

    staging_dir = MakeStagingDirectory(options.staging_dir)

    prev_version = GetPrevVersion(options.build_dir, staging_dir,
                                  options.last_chrome_installer,
                                  options.output_name)

    # Copy the files from the build dir.
    CopyAllFilesToStagingDir(config, options.distribution, staging_dir,
                             options.build_dir, options.enable_hidpi,
                             options.include_snapshotblob, options.include_dxc,
                             options.component_build,
                             options.component_ffmpeg_build, options.verbose)

    if options.component_build == '1':
        DoComponentBuildTasks(staging_dir, options.build_dir,
                              options.target_arch, options.setup_runtime_deps,
                              options.chrome_runtime_deps, current_version)

    version_numbers = current_version.split('.')
    current_build_number = version_numbers[2] + '.' + version_numbers[3]
    prev_build_number = ''
    if prev_version:
        version_numbers = prev_version.split('.')
        prev_build_number = version_numbers[2] + '.' + version_numbers[3]

    # Name of the archive file built (for example - chrome.packed.7z or
    # patch-<old_version>-<new_version>.7z or patch-<new_version>.7z
    archive_file, archive_file_uncompressed = CreateArchiveFiles(
        options, staging_dir, current_build_number, prev_build_number)

    setup_file = PrepareSetupExec(options, current_build_number,
                                  prev_build_number)
    is_component_build = options.component_build == '1'
    if (options.setup_exe_format == 'DIFF'):
        # Create the compressed resource for a differential updater.
        CreateResourceInputFile(options.output_dir, setup_file, 'B7',
                                archive_file, 'B7', options.resource_file_path,
                                is_component_build, staging_dir,
                                current_version)
    elif (options.setup_exe_format == 'FULL'):
        CreateResourceInputFile(options.output_dir, setup_file, 'BN',
                                archive_file, 'B7', options.resource_file_path,
                                is_component_build, staging_dir,
                                current_version)
    else:
        # If `setup_exe_format` does not match 'FULL' or 'DIFF', we assume
        # `COMPRESSED` was intended. May be worth reconsidering this in the
        # future.

        # Create the compressed resource.
        CreateResourceInputFile(options.output_dir, setup_file, 'BL',
                                archive_file, 'B7', options.resource_file_path,
                                is_component_build, staging_dir,
                                current_version)

    # Create the uncompressed resource, if resource_file_path_uncompressed
    # was provided.
    if options.resource_file_path_uncompressed:
        CreateResourceInputFile(options.output_dir, SETUP_EXEC, 'BN',
                                archive_file_uncompressed, 'BN',
                                options.resource_file_path_uncompressed,
                                is_component_build, staging_dir,
                                current_version)


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
    parser.add_option(
        '--resource_file_path',
        help='The path where the compressed resource file will be output. '
        'Defaults to %s in the build directory.' % MINI_INSTALLER_INPUT_FILE)
    parser.add_option(
        '--resource_file_path_uncompressed',
        help='The path where the uncompressed resource file will be output. '
        'This is optional, and should not be used in debug builds. '
        'Defaults to None')
    parser.add_option('-d',
                      '--distribution',
                      help='Name of Chromium Distribution. Optional.')
    parser.add_option('-s',
                      '--skip_rebuild_archive',
                      default="False",
                      help='Skip re-building Chrome.7z archive if it exists.')
    parser.add_option(
        '-l',
        '--last_chrome_installer',
        help='Generate differential installer. The value of this parameter '
        'specifies the directory that contains base versions of '
        'setup.exe, zucchini.exe (if --diff_algorithm is ZUCCHINI) '
        '& chrome.7z.')
    parser.add_option(
        '-f',
        '--setup_exe_format',
        default='COMPRESSED',
        help='How setup.exe should be included {COMPRESSED|DIFF|FULL}.')
    parser.add_option(
        '-a',
        '--diff_algorithm',
        default='BSDIFF',
        help='Diff algorithm to use when generating differential patches '
        '{BSDIFF|ZUCCHINI}.')
    parser.add_option('-n',
                      '--output_name',
                      default='chrome',
                      help='Name used to prefix names of generated archives.')
    parser.add_option('--enable_hidpi',
                      default='0',
                      help='Whether to include HiDPI resource files.')
    parser.add_option('--include_snapshotblob',
                      default='0',
                      help='Whether to include the V8 snapshot blob.')
    parser.add_option('--include_dxc',
                      default='0',
                      help='Whether to include DXC dlls.')
    parser.add_option(
        '--component_build',
        default='0',
        help='Whether this archive is packaging a component build.')
    parser.add_option(
        '--component_ffmpeg_build',
        default='0',
        help='Whether this archive is packaging with ffmpeg component build.')
    parser.add_option(
        '--fast_archive_compression',
        action='store_true',
        default=False,
        help='This will enable fast compression of chrome.7z into '
        'chrome.packed.7z.')
    parser.add_option(
        '--depfile',
        help='Generate a depfile with the given name listing the implicit '
        'inputs to the archive process that can be used with a build system.')
    parser.add_option(
        '--chrome_runtime_deps',
        help='A file listing runtime dependencies. This will be used to get a '
        'list of DLLs to archive in a component build.')
    parser.add_option(
        '--setup_runtime_deps',
        help='A file listing runtime dependencies for setup.exe. This will be '
        'used to get a list of DLLs to archive in a component build.')
    parser.add_option(
        '--target_arch',
        default='x86',
        help='Specify the target architecture for installer - this is used '
        'to determine which CRT runtime files to pull and package '
        'with the installer archive {x86|x64}.')
    parser.add_option(
        '--build_time',
        help='Epoch second in string. If set, overwrite the timestamp for '
        'files archived, to keep output artifacts deterministic.')
    parser.add_option('-v',
                      '--verbose',
                      action='store_true',
                      dest='verbose',
                      default=False)

    options, _ = parser.parse_args()
    if not options.build_dir:
        parser.error('You must provide a build dir.')

    options.build_dir = os.path.normpath(options.build_dir)

    if not options.staging_dir:
        parser.error('You must provide a staging dir.')

    if not options.input_file:
        parser.error('You must provide an input file')

    is_component_build = options.component_build == '1'
    if is_component_build and not options.chrome_runtime_deps:
        parser.error(
            "chrome_runtime_deps must be specified for a component build")
    if is_component_build and not options.setup_runtime_deps:
        parser.error(
            "setup_runtime_deps must be specified for a component build")

    if not options.output_dir:
        options.output_dir = options.build_dir

    if not options.resource_file_path:
        options.resource_file_path = os.path.join(options.build_dir,
                                                  MINI_INSTALLER_INPUT_FILE)

    return options


if '__main__' == __name__:
    options = _ParseOptions()
    if options.verbose:
        print(sys.argv)
    sys.exit(main(options))
