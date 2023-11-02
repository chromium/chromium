# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import sys

import file_operations
import process_operations
import registry_operations


LOGGER = logging.getLogger('installer_test')


def Verify(property_dict, variable_expander):
    """Verifies the expectations in a property dict.

    Args:
        property_dict: A property dictionary mapping type names to expectations.
        variable_expander: A VariableExpander.

    Raises:
        AssertionError: If an expectation is not satisfied.
    """
    _Walk(
        {
            'Files': file_operations.VerifyFileExpectation,
            'Processes': process_operations.VerifyProcessExpectation,
            'RegistryEntries':
            registry_operations.VerifyRegistryEntryExpectation,
        }, False, property_dict, variable_expander)


def Clean(property_dict, variable_expander):
    """Cleans machine state so that expectations will be satisfied.

    Args:
        property_dict: A property dictionary mapping type names to expectations.
        variable_expander: A VariableExpander.
    """
    _Walk(
        {
            'Files': file_operations.CleanFile,
            'Processes': process_operations.CleanProcess,
            'RegistryEntries': registry_operations.CleanRegistryEntry,
        }, True, property_dict, variable_expander)


def _Walk(operations, continue_on_error, property_dict, variable_expander):
    """Traverses |property_dict|, invoking |operations| for each expectation.

    Args:
        operations: A dictionary mapping property dict type names to functions.
        continue_on_error: True if the traversal will log and continue in case
            of exceptions.
        property_dict: A property dictionary mapping type names to expectations.
        variable_expander: A VariableExpander.
    """
    for type_name, expectations in property_dict.items():
        operation = operations[type_name]
        for expectation_name, expectation_dict in expectations.items():
            # Skip over expectations with conditions that aren't satisfied.
            if 'condition' in expectation_dict:
                condition = variable_expander.Expand(
                    expectation_dict['condition'])
                if not _EvaluateCondition(condition):
                    continue
            try:
                operation(expectation_name, expectation_dict,
                          variable_expander)
            except:  # pylint: disable=bare-except
                if not continue_on_error:
                    raise
                LOGGER.error('Error while processing expectation %s: %s' %
                             (expectation_name, sys.exc_info()[1]))


def _EvaluateCondition(condition):
    """Evaluates |condition| using eval().

    Args:
        condition: A condition string.

    Returns:
        The result of the evaluated condition.
    """
    return eval(condition, {'__builtins__': {'False': False, 'True': True}})
