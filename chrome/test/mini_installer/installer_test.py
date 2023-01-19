# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""The primary module for the suite of mini_installer integration tests.

This module houses the InstallerTest class. This is a somewhat special type of
unittest.TestCase that hosts a dynamic set of test functions created from a
JSON configuration file. The module is intended to be loaded by a test process
driven by typ.
"""

import contextlib
import json
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import traceback
import unittest
import win32api
from win32comext.shell import shell, shellcon

from argument_parser import ArgumentParser
import property_walker
from variable_expander import VariableExpander

CUR_DIR = os.path.dirname(os.path.realpath(__file__))

# Auto-detect if the tests are not being run on the bots.
RUNNING_LOCALLY = (os.getenv('SWARMING_HEADLESS') != '1'
                   and os.getenv('CHROME_HEADLESS') != '1')

# The logger configured in this module and used by its dependents.
LOGGER = logging.getLogger('installer_test')

_force_clean = not RUNNING_LOCALLY


class Config:
    """Describes the machine states, actions, and test cases.

    Attributes:
        states: A dictionary where each key is a state name and the associated
            value is a property dictionary describing that state.
        actions: A dictionary where each key is an action name and the
            associated value is the action's command.
        tests: An array of test cases.
        traversals: Maps test names to their traversals.
    """

    def __init__(self):
        self.states = {}
        self.actions = {}
        self.tests = []
        self.traversals = {}


class InstallerTest(unittest.TestCase):
    """Tests a test case in the config file."""

    _config = None
    _output_dir = None
    _variable_expander = None

    def __init__(self, method_name):
        """Constructor.

        Args:
            method_name: The name of this test.
        """
        assert InstallerTest._config, 'module _initialize() not yet called'
        super().__init__(method_name)
        self._name = method_name[5:]
        self._test = InstallerTest._config.traversals[self._name]
        self._clean_on_teardown = True
        self._log_path = None

    def __str__(self):
        """Returns a string representing the test case.

        Returns:
            A string created by joining state names and action names together
            with ' -> ', for example, 'Test: clean -> install chrome ->
            chrome_installed'.
        """
        return '%s: %s\n' % (self._name, ' -> '.join(self._test))

    def setUp(self):
        # Create a temp file to contain the installer log(s) for this test.
        log_file, self._log_path = tempfile.mkstemp()
        os.close(log_file)
        self.addCleanup(os.remove, self._log_path)
        InstallerTest._variable_expander.SetLogFile(self._log_path)
        self.addCleanup(InstallerTest._variable_expander.SetLogFile, None)

    def run_test(self):
        """Run the test case."""
        # |test| is an array of alternating state names and action names,
        # starting and ending with state names. Therefore, its length must be
        # odd.
        self.assertEqual(1,
                         len(self._test) % 2,
                         'The length of test array must be odd')

        state = self._test[0]
        self._VerifyState(state)

        # Starting at index 1, we loop through pairs of (action, state).
        for i in range(1, len(self._test), 2):
            action = self._test[i]
            LOGGER.info('Beginning action %s' % action)
            RunCommand(InstallerTest._config.actions[action],
                       InstallerTest._variable_expander)
            LOGGER.info('Finished action %s' % action)

            state = self._test[i + 1]
            self._VerifyState(state)

        # If the test makes it here, it means it was successful, because
        # RunCommand and _VerifyState throw an exception on failure.
        self._clean_on_teardown = False

    def tearDown(self):
        """Cleans up the machine if the test case fails."""
        if self._clean_on_teardown:
            # The last state in the test's traversal is its "clean" state, so
            # use it to drive the cleanup operation.
            clean_state_name = self._test[len(self._test) - 1]
            RunCleanCommand(True,
                            InstallerTest._config.states[clean_state_name],
                            InstallerTest._variable_expander)
            # Either copy the log to isolated outdir or dump it to console.
            if InstallerTest._output_dir:
                target = os.path.join(InstallerTest._output_dir,
                                      os.path.basename(self._log_path))
                shutil.copyfile(self._log_path, target)
                LOGGER.error('Saved installer log to %s', target)
            else:
                with open(self._log_path) as fh:
                    LOGGER.error(fh.read())

    def shortDescription(self):
        """Overridden from unittest.TestCase.

        We return None as the short description to suppress its printing.
        The default implementation of this method returns the docstring of the
        runTest method, which is not useful since it's the same for every test
        case. The description from the __str__ method is informative enough.
        """
        return None

    def _VerifyState(self, state):
        """Verifies that the current machine state matches a given state.

        Args:
            state: A state name.
        """
        LOGGER.info('Verifying state %s' % state)
        try:
            property_walker.Verify(InstallerTest._config.states[state],
                                   InstallerTest._variable_expander)
        except AssertionError as e:
            # If an AssertionError occurs, we intercept it and add the state
            # name to the error message so that we know where the test fails.
            raise AssertionError("In state '%s', %s" % (state, str(e))) from e


def RunCommand(command, variable_expander):
    """Runs the given command from the current file's directory.

    This function throws an Exception if the command returns with non-zero exit
    status.

    Args:
        command: A command to run. It is expanded using Expand.
        variable_expander: A VariableExpander object.
    """
    expanded_command = variable_expander.Expand(command)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    returncode = None
    stdout = ''
    stderr = ''
    # Uninstall is special in that it is run in interactive mode and may need
    # user input. This needs to happen even if the quiet arg is passed to
    # prevent a deadlock
    if 'uninstall_chrome.py' in expanded_command:
        returncode = subprocess.call(expanded_command,
                                     shell=True,
                                     cwd=script_dir)
    else:
        proc = subprocess.Popen(expanded_command,
                                shell=True,
                                cwd=script_dir,
                                text=True,
                                encoding='utf-8',
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)
        stdout, stderr = proc.communicate()
        returncode = proc.returncode

    if stdout:
        LOGGER.info('stdout:\n%s', stdout.replace('\r', '').rstrip('\n'))
    if stderr:
        LOGGER.error('stdout:\n%s', stderr.replace('\r', '').rstrip('\n'))
    if returncode != 0:
        raise Exception('Command %s returned non-zero exit status %s' %
                        (expanded_command, returncode))


def RunCleanCommand(force_clean, clean_state, variable_expander):
    """Puts the machine in the clean state (e.g. Chrome not installed).

    Args:
        force_clean: A boolean indicating whether to force cleaning existing
            installations.
        clean_state: The state used to verify a clean machine after each test.
            This state is used to drive the cleanup operation.
        variable_expander: A VariableExpander object.
    """
    # A list of (product_name, product_switch) tuples for the possible installed
    # products.
    data = [('$CHROME_LONG_NAME', '')]
    # Chrome for Testing does not support system-level installs.
    if variable_expander.Expand('$BRAND') != 'Google Chrome for Testing':
        data.extend([('$CHROME_LONG_NAME', '--system-level')])
    if variable_expander.Expand('$BRAND') == 'Google Chrome':
        data.extend([('$CHROME_LONG_NAME_BETA', ''),
                     ('$CHROME_LONG_NAME_BETA', '--system-level'),
                     ('$CHROME_LONG_NAME_DEV', ''),
                     ('$CHROME_LONG_NAME_DEV', '--system-level'),
                     ('$CHROME_LONG_NAME_SXS', '')])

    # Attempt to run each installed product's uninstaller.
    interactive_option = '--interactive' if not force_clean else ''
    for product_name, product_switch in data:
        command = (
            '%s uninstall_chrome.py '
            '--chrome-long-name="%s" '
            '--no-error-if-absent %s %s' %
            (sys.executable, product_name, product_switch, interactive_option))
        try:
            RunCommand(command, variable_expander)
        except:  # pylint: disable=bare-except
            message = traceback.format_exception(*sys.exc_info())
            message.insert(0, 'Error cleaning up an old install with:\n')
            LOGGER.info(''.join(message))
            if not force_clean:
                raise

    # Once everything is uninstalled, make a pass to delete any stray tidbits on
    # the machine.
    if force_clean:
        property_walker.Clean(clean_state, variable_expander)


def MergePropertyDictionaries(current_property, new_property):
    """Merges the new property dictionary into the current property dictionary.

    This is different from general dictionary merging in that, in case there are
    keys with the same name, we merge values together in the first level, and we
    override earlier values in the second level. For more details, take a look
    at http://goo.gl/uE0RoR.

    Args:
        current_property: The property dictionary to be modified.
        new_property: The new property dictionary.
    """
    for key, value in new_property.items():
        if key not in current_property:
            current_property[key] = value
        else:
            assert (isinstance(current_property[key], dict)
                    and isinstance(value, dict))
            # This merges two dictionaries together. In case there are keys with
            # the same name, the latter will override the former.
            current_property[key].update(value)


def FilterConditionalElem(elem, condition_name, variable_expander):
    """Returns True if a conditional element should be processed.

    Args:
        elem: A dictionary.
        condition_name: The name of the condition property in |elem|.
        variable_expander: A variable expander used to evaluate conditions.

    Returns:
        True if |elem| should be processed.
    """
    if condition_name not in elem:
        return True
    condition = variable_expander.Expand(elem[condition_name])
    return eval(condition, {'__builtins__': {'False': False, 'True': True}})


def ParsePropertyFiles(directory, filenames, variable_expander):
    """Parses an array of .prop files.

    Args:
        directory: The directory where the Config file and all Property files
            reside in.
        filenames: An array of Property filenames.
        variable_expander: A variable expander used to evaluate conditions.

    Returns:
        A property dictionary created by merging all property dictionaries
        specified in the array.
    """
    current_property = {}
    for filename in filenames:
        path = os.path.join(directory, filename)
        new_property = json.load(open(path))
        if not FilterConditionalElem(new_property, 'Condition',
                                     variable_expander):
            continue
        # Remove any Condition from the property dict before merging since it
        # serves no purpose from here on out.
        if 'Condition' in new_property:
            del new_property['Condition']
        MergePropertyDictionaries(current_property, new_property)
    return current_property


def ParseConfigFile(filename, variable_expander):
    """Parses a .config file.

    Args:
        config_filename: A Config filename.

    Returns:
        A Config object.
    """
    with open(filename, 'r') as fp:
        config_data = json.load(fp)
    directory = os.path.dirname(os.path.abspath(filename))

    config = Config()
    config.tests = config_data['tests']
    # Drop conditional tests that should not be run in the current
    # configuration.
    config.tests = list(
        filter(
            lambda t: FilterConditionalElem(t, 'condition', variable_expander),
            config.tests))
    for state_name, state_property_filenames in config_data['states']:
        config.states[state_name] = ParsePropertyFiles(
            directory, state_property_filenames, variable_expander)
    for action_name, action_command in config_data['actions']:
        config.actions[action_name] = action_command
    for test in config.tests:
        config.traversals[test['name']] = test['traversal']
    return config


@contextlib.contextmanager
def ConfigureTempOnDrive(drive):
    """Ensures that TMP is on |drive|, restoring state on completion.

    This does not change the current Python runtime's idea of tempdir.
    """
    tmp_set = False
    old_tmp = None
    tmp_created = None
    # Set TMP to something reasonable if the default temp path is not on the
    # desired drive. Note that os.environ is used to mutate the environment
    # since doing so writes through to the process's underlying environment
    # block. Reads are performed directly in the off chance that code elsewhere
    # has modified the environment without going through os.environ.
    temp = win32api.GetTempPath()
    if not temp or os.path.splitdrive(temp)[0] != drive:
        # Try to use one of the standard Temp dir locations.
        for candidate in [os.getenv(v) for v in ['LOCALAPPDATA', 'windir']]:
            if candidate and os.path.splitdrive(candidate)[0] == drive:
                temp = os.path.join(candidate, 'Temp')
                if os.path.isdir(temp):
                    old_tmp = os.getenv('TMP')
                    os.environ['TMP'] = temp
                    tmp_set = True
                    break
        # Otherwise make a Temp dir at the root of the drive.
        if not tmp_set:
            temp = os.path.join(drive, os.sep, 'Temp')
            if not os.path.exists(temp):
                os.mkdir(temp)
                tmp_created = temp
            elif not os.path.isdir(temp):
                raise Exception(
                    'Cannot create %s without clobbering something' % temp)
            old_tmp = os.getenv('TMP')
            os.environ['TMP'] = temp
            tmp_set = True
    try:
        yield
    finally:
        if tmp_set:
            if old_tmp is None:
                del os.environ['TMP']
            else:
                os.environ['TMP'] = old_tmp
        if tmp_created:
            shutil.rmtree(tmp_created, True)
            if os.path.isdir(tmp_created):
                raise Exception('Failed to entirely delete directory %s' %
                                tmp_created)


def GetAbsoluteExecutablePath(path):
    """Gets the absolute path to the an executable.

    The path can either be an absolute or relative path, as well as the
    executable's name. These are used to probe user-specified and common
    binary paths.

    This method searches for the binary in common locations:
    - path location (when specifying a non-standard location)
    - out\Release\path (local default path, path here is the filename)
    - out\Default\path (alternate local default path)
    - out\Release_x64\path (on waterfall)


    Args:
        path: The path to the file. This can be an absolute or relative path.

    Returns:
        Absolute path to installer.
    """
    possible_paths = [
        os.path.abspath(os.path.join(path)),
        os.path.abspath(os.path.join('out', 'Release', path)),
        os.path.abspath(os.path.join('out', 'Release_x64', path)),
        os.path.abspath(os.path.join('out', 'Default', path)),
    ]
    for _path in possible_paths:
        if os.path.exists(_path):
            return _path
    raise RuntimeError('Binary can\'t be found: %s' % path)


def GetAbsoluteConfigPath(path):
    """Returns the absolute path to the config file.

    Args:
        path: The path to the file.

    Returns:
        Absolute path to the config file.
    """
    if not os.path.exists(path):
        path = os.path.join(CUR_DIR, 'config', path)
    assert os.path.exists(path), 'Config not found at %s' % path
    LOGGER.info('Config found at %s', path)
    return os.path.abspath(path)


def setUpModule():
    # Make sure that TMP and Chrome's installation directory are on the same
    # drive to work around https://crbug.com/700809. (CSIDL_PROGRAM_FILESX86 is
    # valid for both 32 and 64-bit apps running on 32 or 64-bit Windows.)
    drive = os.path.splitdrive(
        shell.SHGetFolderPath(0, shellcon.CSIDL_PROGRAM_FILESX86, None, 0))[0]
    _temp_dir_manager = ConfigureTempOnDrive(drive)
    _temp_dir_manager.__enter__()  # pylint: disable=no-member
    unittest.addModuleCleanup(_temp_dir_manager.__exit__, None, None, None)  # pylint: disable=no-member

    # The last state in any test's traversal is the "clean" state, so use it to
    # drive the initial cleanup operation.
    a_test = InstallerTest._config.tests[0]['traversal']
    clean_state_name = a_test[len(a_test) - 1]
    clean_state = InstallerTest._config.states[clean_state_name]
    try:
        RunCleanCommand(_force_clean, clean_state,
                        InstallerTest._variable_expander)
    except:
        _temp_dir_manager.__exit__(None, None, None)  # pylint: disable=no-member
        _temp_dir_manager = None
        raise


def _initialize():
    """Initializes the InstallerTest class.

    This entails setting the class attributes and adding the configured test
    methods to the class.
    """
    args = ArgumentParser().parse_args()

    log_level = (logging.ERROR if args.quiet else
                 logging.DEBUG if args.verbose else logging.INFO)
    LOGGER.setLevel(log_level)
    handler = logging.StreamHandler()
    handler.setFormatter(
        logging.Formatter(
            fmt='[%(asctime)s:%(filename)s(%(lineno)d)] %(message)s',
            datefmt='%m%d/%H%M%S'))
    LOGGER.addHandler(handler)

    # Pull args from the parent proc out of the environment block.
    if os.environ.get('CMI_FORCE_CLEAN', False):
        global _force_clean
        _force_clean = True
    InstallerTest._output_dir = os.environ.get('CMI_OUTPUT_DIR')
    installer_path = GetAbsoluteExecutablePath(
        os.environ.get('CMI_INSTALLER_PATH', 'mini_installer.exe'))
    previous_version_installer_path = GetAbsoluteExecutablePath(
        os.environ.get('CMI_PREVIOUS_VERSION_INSTALLER_PATH',
                       'previous_version_mini_installer.exe'))
    chromedriver_path = GetAbsoluteExecutablePath(
        os.environ.get('CMI_CHROMEDRIVER_PATH', 'chromedriver.exe'))
    config_path = GetAbsoluteConfigPath(
        os.environ.get('CMI_CONFIG', 'config.config'))

    InstallerTest._variable_expander = VariableExpander(
        installer_path, previous_version_installer_path, chromedriver_path,
        args.quiet, InstallerTest._output_dir)
    InstallerTest._config = ParseConfigFile(config_path,
                                            InstallerTest._variable_expander)

    # Add a test_Foo function to the InstallerTest class for each test in
    # the config file.
    run_test_fn = getattr(InstallerTest, 'run_test')
    for test in InstallerTest._config.tests:
        setattr(InstallerTest, 'test_' + test['name'], run_test_fn)


_initialize()
del _initialize
