#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Parsing logic to read files for the Web App testing framework.
"""

import logging
import re
from typing import Dict, List, Set, Tuple

from classes import Action
from classes import CoverageTest
from classes import PartialCoverageAddition


def HumanFriendlyNameToCanonicalActionName(
        human_friendly_action_name: str,
        action_base_name_to_default_param: Dict[str, str]):
    """
    Converts a human-friendly action name and turns into the format compatible
    with this testing framework. This does two things:
    1) Resolving specified parameters, from "()" format into a "_". For example,
       "action(with_parameter)" turns into "action_with_parameter".
    2) Resolving parameterless actions (that have a default parameter) to
       include the default parameter. For example, if
       |action_base_name_to_default_param| contains an entry for
       |human_friendly_action_name|, then that entry is appended to the action.
       "action" and {"action": "default_param"} will respectively will return
       "action_default_param".
    If neither of those cases apply, then the |human_friendly_action_name| is
    returned.
    """
    human_friendly_action_name = human_friendly_action_name.strip()
    if human_friendly_action_name in action_base_name_to_default_param:
        # Handle default parameter.
        human_friendly_action_name += "_" + action_base_name_to_default_param[
            human_friendly_action_name]
    elif '(' in human_friendly_action_name:
        # Handle parameter being specified.
        human_friendly_action_name = human_friendly_action_name.replace(
            "(", "_").rstrip(")")
    return human_friendly_action_name


def ReadActionsFile(
        actions_csv_file
) -> Tuple[Dict[str, Action], Dict[str, str], Dict[str, Set[str]]]:
    """Reads the actions comma-separated-values file.

    If parameters are specified for an action in the file, then one action is
    added to the results dictionary per action_base_name + parameter
    combination. A parameter marked with a "*" is considered the default
    parameter for that action.

    See the README.md for more information about actions and action templates.

    Args:
        actions_csv_file: The comma-separated-values file read to parse all
                          actions.

    Returns (actions_by_name,
             action_base_name_to_default_param,
             action_base_name_to_all_params):
        actions_by_name:
            Index of all actions by action name.
        action_base_name_to_default_param:
            Index of action base names to the default parameter. Only populated
            for actions with default parameters.
        action_base_name_to_all_params:
            Index of action base names to all the parameters, if it has
            parameters.

    Raises:
        ValueError: The input file is invalid.
    """
    actions_by_name = {}
    action_base_name_to_default_param = {}
    action_base_name_to_all_params = {}
    for i, row in enumerate(actions_csv_file):
        if not row:
            continue
        if row[0].startswith("#"):
            continue
        if len(row) < 3:
            raise ValueError(f"Row {i!r} does not contain enough entries. "
                             f"Got [{','.join(row)!r}].")
        action_base_name = row[0].strip()
        if not re.fullmatch(r'\w+', action_base_name):
            raise ValueError(
                f"Invald action base name {action_base_name!r} on "
                f"row {i!r}.")
        params = [param.strip() for param in row[1].split("| ")]
        is_state_check_text = row[2].strip()
        if is_state_check_text not in ("", "Y"):
            raise ValueError(
                f"Row {i!r} signifying if {action_base_name!r} is an "
                f"state_check action must be either empty or 'Y'.")
        is_state_check = is_state_check_text == "Y"
        if not params:
            params = [""]
        action_base_name_to_all_params[action_base_name] = set()
        for param in params:
            if not re.fullmatch(r'(\w+\*?)|', param):
                raise ValueError(f"Invald action param name {param!r}) on row "
                                 f"{i!r}.")
            if "*" in param:
                action_base_name_to_default_param[
                    action_base_name] = param.rstrip("*")
            param = param.rstrip("*")
            name = action_base_name
            if param != "":
                action_base_name_to_all_params[action_base_name].add(param)
                name += "_" + param
            if name in actions_by_name:
                raise ValueError(
                    f"Cannot add duplicate action {name!r} on row "
                    f"{i!r}")
            action = Action(name, action_base_name, is_state_check)
            actions_by_name[action.name] = action
    return (actions_by_name, action_base_name_to_default_param,
            action_base_name_to_all_params)


def ReadCoverageTestsFile(coverage_csv_file, actions: Dict[str, Action],
                          action_base_name_to_default_param: Dict[str, str]
                          ) -> List[CoverageTest]:
    """Reads the coverage tests comma-separated-values file.

    The coverage tests file can have blank entries in the test row, and does not
    have test names.

    Args:
        coverage_csv_file: The comma-separated-values file with all coverage
                           tests.
        actions: An index of action name to Action
        action_base_name_to_default_param: An index of action base name to
                                           default parameter, if there is one.

    Returns:
        A list of CoverageTests read from the file.

    Raises:
        ValueError: The input file is invalid.
    """
    missing_actions = []
    required_coverage_tests = []
    for i, row in enumerate(coverage_csv_file):
        if not row:
            continue
        if row[0].startswith("#"):
            continue
        if len(row) < 3:
            raise ValueError(f"Row {i!r} does not have test actions.")
        coverage_test = CoverageTest(str(i))
        required_coverage_tests.append(coverage_test)
        for action_name in row[2:]:
            action_name = action_name.strip()
            if "," in action_name:
                raise ValueError(f"Actions on row {i!r} cannot have "
                                 f"multiple parameters: {action_name!r}")
            if action_name == "":
                continue
            action_name = HumanFriendlyNameToCanonicalActionName(
                action_name, action_base_name_to_default_param)
            if action_name not in actions:
                missing_actions.append(action_name)
                logging.error(f"Could not find action on row {i!r}: "
                              f"{action_name!r}")
                continue
            coverage_test.actions.append(actions[action_name])
    if missing_actions:
        raise ValueError(f"Actions missing from actions dictionary: "
                         f"{', '.join(missing_actions)!r}")
    return required_coverage_tests


def ReadNamedTestsFile(tests_csv_file, actions: Dict[str, Action],
                       action_base_name_to_default_param: Dict[str, str]
                       ) -> List[CoverageTest]:
    """Reads the tests comma-separated-values file.

    Test rows are expected to begin with the test name. Tests rows with no
    actions are ignored, and if a test has "N/A" as an action, then no more
    actions are parsed for that test.

    Args:
        tests_csv_file: The comma-separated-values file with tests.
        actions: An index of action name to Action.
        action_base_name_to_default_param: An index of action base name to
                                           default parameter, if there is one.
    Returns:
        A list of CoverageTests read from the file.

    Raises:
        ValueError: The input file is invalid.
    """
    missing_actions = []
    tests = []
    # Create a test for each row, and populate all actions for that test.
    for row in tests_csv_file:
        if not row:
            continue
        if row[0].startswith("#"):
            continue
        if len(row) < 2:
            continue
        test = CoverageTest(row[0])
        for action_name in row[1:]:
            action_name = action_name.strip()
            if action_name == "" or action_name == "N/A":
                break
            action_name = HumanFriendlyNameToCanonicalActionName(
                action_name, action_base_name_to_default_param)
            if action_name not in actions:
                missing_actions.append(action_name)
                logging.error(f"Could not find action: {action_name!r}")
                continue
            test.actions.append(actions[action_name])
        if test.actions:
            tests.append(test)
    if missing_actions:
        raise ValueError(f"Actions missing from actions dictionary: "
                         f"{', '.join(missing_actions)!r}")
    return tests


def ValidatePartialPaths(partial_paths: List[PartialCoverageAddition]):
    """Validates that all |partial_paths| are valid:
    1) They contain actions, and
    2) They don't share an initial non-state-check action. This means
       that the first action in the input and output actions that is
       not a "state check" action must not be the same.
    """
    for partial_path in partial_paths:
        assert len(partial_path.input_actions) > 0
        assert len(partial_path.output_actions) > 0

        def FindFirstNonStateCheckAction(actions: List[Action]) -> Action:
            for action in actions:
                if not action.is_state_check:
                    return action
            return None

        # Check that the paths don't both start with the same non-state_check
        # action.
        first_input_non_state_check_action = FindFirstNonStateCheckAction(
            partial_path.input_actions)
        first_output_non_state_check_action = FindFirstNonStateCheckAction(
            partial_path.output_actions)
        if (first_input_non_state_check_action is not None
                and first_output_non_state_check_action is not None
                and first_input_non_state_check_action is
                first_output_non_state_check_action):
            raise ValueError(
                f"Partial path addition cannot share the same first "
                f"non-state_check action in the input and output actions "
                f"{first_input_non_state_check_action.name!r}.")


def ReadPartialCoveragePathsFile(
        partial_paths_csv_file, actions: Dict[str, Action],
        action_base_name_to_all_params: Dict[str, Set[str]]
) -> List[PartialCoverageAddition]:
    """Reads the partial coverage paths file.

    The file is expected to have 2 columns per row, each one a semicolon-
    separated list of action names. Actions can be non-parameterized, in which
    case a partial path for every parameter will be added. If multiple actions
    have unspecified parameters (on input & output), then parameters are chosen
    as an intersection of all parameter options. If no parameters are possible,
    an exception is raised.

    The paths cannot share initial non-state_check actions.

    Args:
        partial_paths_csv_file: The comma-separated-values file with all partial
                                paths.
        actions: An index of action name to Action.
        action_base_name_to_all_params: An index of action base name to all
                                        parameters.

    Returns:
        A list of PartialCoverageAddition objects.

    Raises:
        ValueError: The input file is invalid.
    """
    partial_paths: List[PartialCoverageAddition] = []
    for row in partial_paths_csv_file:
        if not row:
            continue
        if row[0].startswith("#"):
            continue
        if len(row) < 2:
            continue
        input_action_names = [name.strip() for name in row[0].split(';')]
        output_action_names = [name.strip() for name in row[1].split(';')]

        if not input_action_names or not output_action_names:
            raise ValueError(f"Input and output actions must be populated: "
                             f"[{'; '.join(row)!r}]")

        # To support parameters, find all of the common parameters for all
        # actions that have parameters, and output a separate partial path
        # per parameter matching.
        params_needed = False
        all_params = None

        for action_name in input_action_names + output_action_names:
            if action_name in actions:
                continue
            if action_name not in action_base_name_to_all_params:
                raise ValueError(
                    f"Could not find action or action base {action_name!r}")
            params_needed = True
            action_params = action_base_name_to_all_params[action_name]
            assert isinstance(action_params, set)
            all_params = (action_params if all_params == None else
                          all_params.intersection(action_params))

        if params_needed and not all_params:
            raise ValueError(
                f"Could not find common parameters in input actions "
                f"[{', '.join(input_action_names)!r}] and output actions "
                f"[{', '.join(output_action_names)!r}].")

        if not params_needed:
            partial = PartialCoverageAddition()
            partial.input_actions = ([
                actions[action_name] for action_name in input_action_names
            ])
            partial.output_actions = ([
                actions[action_name] for action_name in output_action_names
            ])
            partial_paths.append(partial)
            continue

        assert isinstance(all_params, set)
        for param in all_params:
            input_action_names_with_param = []
            output_action_names_with_param = []
            for action_name in input_action_names:
                if action_name not in actions:
                    action_name = action_name + "_" + param
                if action_name not in actions:
                    raise ValueError(f"Could not find action {action_name!r}")
                input_action_names_with_param.append(action_name)
            for action_name in output_action_names:
                if action_name not in actions:
                    action_name = action_name + "_" + param
                if action_name not in actions:
                    raise ValueError(f"Could not find action {action_name!r}")
                output_action_names_with_param.append(action_name)
            partial = PartialCoverageAddition()
            partial.input_actions = ([
                actions[action_name]
                for action_name in input_action_names_with_param
            ])
            partial.output_actions = ([
                actions[action_name]
                for action_name in output_action_names_with_param
            ])
            partial_paths.append(partial)
    ValidatePartialPaths(partial_paths)
    return partial_paths


def ReadFrameworkActions(framework_actions_csv_file,
                         actions: Dict[str, Action],
                         action_base_name_to_default_param: Dict[str, str]
                         ) -> Dict[str, Action]:
    """Reads the supported framework actions file.

    Ignores blank actions.

    Args:
        framework_actions_csv_file: The comma-separated-values file with all
                                    supported framework actions.
        actions: An index of action name to Action.
        action_base_name_to_default_param: An index of action base name default
                                           parameter.

    Returns:
        A dictionary of actions supported by the testing framework.

    Raises:
        ValueError: The input file is invalid.
    """
    missing_actions = []
    framework_actions = {}
    for row in framework_actions_csv_file:
        if not row:
            continue
        if row[0].startswith("#"):
            continue
        action_name = row[0].strip()
        if not action_name:
            continue
        action_name = HumanFriendlyNameToCanonicalActionName(
            action_name, action_base_name_to_default_param)
        if action_name not in actions:
            missing_actions.append(action_name)
            logging.error(f"Could not find action: {action_name!r}")
            continue
        framework_actions[action_name] = actions[action_name]
    if missing_actions:
        raise ValueError(f"Actions missing from actions dictionary: "
                         f"{', '.join(missing_actions)!r}")
    return framework_actions
