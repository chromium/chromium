# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import sys

import winreg

import ui

# UAC window title constants
_UAC_DIALOG_TITLE = 'User Account Control'

_REG_VALUE_ENABLE_LUA = 'EnableLUA'
_REG_VALUE_PROMPT_ON = 'PromptOnSecureDesktop'
_REG_VALUE_PROMPT_CONSENT = 'ConsentPromptBehaviorAdmin'


def _QueryPolicyValue(value_name, expected_type=winreg.REG_DWORD):
    """Queries the system policy value from registry.

    Args:
        value_name: Registry value name for the policy.
        expected_type: Expected registry value data type.

    Returns:
        The policy value in its desired data type, or None if no such policy or
        data type is not expected.
    """
    system_policy_path = (r'Software\Microsoft\Windows'
                          r'\CurrentVersion\Policies\System')

    try:
        hklm = winreg.ConnectRegistry(None, winreg.HKEY_LOCAL_MACHINE)
        policy_key = winreg.OpenKeyEx(hklm, system_policy_path)
        value, data_type = winreg.QueryValueEx(policy_key, value_name)
        return value if data_type == expected_type else None
    except FileNotFoundError:
        return None


def IsPromptingOnSecureDesktop():
    """Checks whether UAC will be prompted to the secure desktop."""
    prompt_location_policy = _QueryPolicyValue(_REG_VALUE_PROMPT_ON)
    return prompt_location_policy is None or bool(prompt_location_policy)


def IsSupported():
    """Checks whether current system supports UAC.

    Returns:
        True if system supports UAC (after XP), otherwise False.
    """
    return sys.getwindowsversion()[0] > 5


def IsLuaEnabled():
    """Checks whether LUA is enabled on the machine.

    Returns:
        True if LUA is enable, False otherwise.
    """
    enable_lua = _QueryPolicyValue(_REG_VALUE_ENABLE_LUA)
    return enable_lua is None or bool(enable_lua)


def IsElevationSilent():
    """Checks whether user can elevate silently (without UAC prompt).

    Returns:
        True if silent elevation is possible, False otherwise.
    """
    prompt_behavior = _QueryPolicyValue(_REG_VALUE_PROMPT_CONSENT)

    if prompt_behavior == 0:
        logging.info('Silent UAC elevation is enabled.')
        return True
    else:
        logging.info('UAC prompt must be explicitly clicked.')
        return False


def IsEnabled():
    """Checks whether UAC is supported and enabled on current system."""
    uac_enabled = IsSupported() and IsLuaEnabled() and not IsElevationSilent()
    logging.info('UAC is %s.', 'enabled' if uac_enabled else 'NOT enabled')
    return uac_enabled


def AnswerUpcomingUACPrompt(allow=True, timeout=30):
    """Answer upcoming UAC prompt that does not require username/password.

    Args:
        allow: Answer allow or not to the prompt.
        timeout: Wait timeout value in seconds.

    Returns:
        True if UAC prompt clicked as requested.
    """
    logging.info('Waiting at most %s seconds for UAC prompt...', timeout)
    uac_hwnd = ui.WaitForWindow(_UAC_DIALOG_TITLE, None, timeout)[0]

    if not uac_hwnd:
        logging.warning('UAC prompt not found in %f seconds.', timeout)
        return False
    else:
        logging.info('UAC prompt appeared.')

    # We assume the UAC prompt does not require credentials.
    # Press shortcut key Alt+Y or ESC to allow or deny UAC request.
    logging.info('%s UAC prompt.', 'Accepting' if allow else 'Denying')
    key_to_send = '%y' if allow else '{ESC}'
    return ui.SendKeyToWindow(uac_hwnd, key_to_send)
