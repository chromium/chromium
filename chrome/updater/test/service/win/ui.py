# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

import pythoncom
import pywintypes
import win32api
import win32com
import win32com.client
import win32con
import win32gui
import win32process
import winerror


class _MessageQueueAttacher(object):
    """Wrapper class for message queue attachment."""

    def __enter__(self):
        """Attaches the current thread to the foreground window's message queue.

        This is an old and well known exploit used to bypass Windows Focus
        rules:
        http://www.google.com/search?q=attachthreadinput+setforegroundwindow
        """
        self._active_thread_id = 0
        active_hwnd = win32gui.GetForegroundWindow()
        if not active_hwnd:
            logging.warning('No active window is found.')
            return
        current_thread_id = win32api.GetCurrentThreadId()
        active_thread_id, _ = win32process.GetWindowThreadProcessId(
            active_hwnd)
        win32process.AttachThreadInput(current_thread_id, active_thread_id, 1)
        logging.info('Attached current thread input %s to active thread: %s',
                     current_thread_id, active_thread_id)
        self._active_thread_id = active_thread_id

    def __exit__(self, unused_type, unused_value, unused_traceback):
        """Detaches the current thread from the active thread's message queue.
        """
        if not self._active_thread_id:
            return
        current_thread_id = win32api.GetCurrentThreadId()
        win32process.AttachThreadInput(current_thread_id,
                                       self._active_thread_id, 0)
        logging.info('Detached current thread input %s from thread: %s',
                     current_thread_id, self._active_thread_id)


def SetForegroundWindow(hwnd):
    """Brings the given window to foreground.

    Args:
        hwnd: Handle of the window to bring to the foreground.
    """
    with _MessageQueueAttacher():
        return bool(win32gui.SetForegroundWindow(hwnd))


def FindWindowsWithText(parent, text_to_search):
    """Finds windows with given text.

    Args:
        parent: Handle to the parent window whose child windows are to be
                searched.
        text_to_search: Substring to search within Windows text,
                        case-insensitive.

    Returns:
        A list of HWND that match the search condition.
    """

    class WindowFoundHandler(object):
        """Callback class for window enumeration."""

        def __init__(self, text_to_search):
            self.result = []
            self._text_to_search = text_to_search

        def Process(self, handle):
            """Callback function when enumerating a window.

      Args:
          handle: HWND to the enumerated window.
      """
            text = win32gui.GetWindowText(handle).lower()
            text_to_search = self._text_to_search.lower()

            if text_to_search in text:
                self.result.append(handle)

    def WinFoundCallback(hwnd, window_found_handler):
        window_found_handler.Process(hwnd)

    handler = WindowFoundHandler(text_to_search)
    try:
        win32gui.EnumChildWindows(parent, WinFoundCallback, handler)
    except pywintypes.error as e:
        logging.info('Error while searching [%s], error: [%s]', text_to_search,
                     e)

    return handler.result


def FindWindowsWithTitle(title_to_search):
    """Finds windows with given title.

    Args:
        title_to_search: Window title substring to search, case-insensitive.

    Returns:
        A list of HWND that match the search condition.
    """
    desktop_handle = None
    return FindWindowsWithText(desktop_handle, title_to_search)


def FindWindow(title, class_name, parent=0, child_after=0):
    """Finds a window of a given title and class.

    Args:
        title: Title of the window to search.
        class_name: Class name of the window to search.
        parent: Handle to the parent window whose child windows are to be
                searched.
        child_after: HWND to a child window. Search begins with the next child
          window in the Z order.

    Returns:
        Handle of the found window, or 0 if not found.
    """
    hwnd = 0
    try:
        hwnd = win32gui.FindWindowEx(int(parent), int(child_after), class_name,
                                     title)
    except win32gui.error as err:
        if err[0] == winrror.ERROR_INVALID_WINDOW_HANDLE:  # Could be closed.
            pass
        elif err[0] != winrror.ERROR_FILE_NOT_FOUND:
            raise err
    if hwnd:
        win32gui.FlashWindow(hwnd, True)
    return hwnd


def FindWindowWithTitleAndText(title, text):
    """Checks if the any window has given title, and child window has the text.

    Args:
        title: Expected window title.
        text: Expected window text substring(in any child window),
              case-insensitive.

    Returns:
        A list how HWND that meets the search condition.
    """
    hwnds_title_matched = FindWindowsWithTitle(title)
    if not hwnds_title_matched:
        logging.info('No window has title: [%s].', title)

    hwnds = []
    for hwnd in hwnds_title_matched:
        if FindWindowsWithText(hwnd, text):
            hwnds.append(hwnd)
    return hwnds


def WaitForWindow(title, class_name, timeout=30):
    """Waits for window with given title and class to appear.

    Args:
        title: Windows title to search.
        class_name: Class name of the window to search.
        timeout: How long should wait before give up.

    Returns:
        A tuple of (HWND, title) of the found window, or (None, None) otherwise.
    """
    logging.info('ui.WaitForWindow("%s", "%s") for %s seconds', title,
                 class_name, timeout)
    hwnd = None
    start = time.perf_counter()
    stop = start + int(timeout)

    while time.perf_counter() < stop:
        hwnd = FindWindow(title, class_name)
        if hwnd:
            elapsed = time.perf_counter() - start
            logging.info('Window ["%s"] found in %f seconds', title, elapsed)
            return (hwnd, title)
        logging.info('Window with title [%s] has not appeared yet.', title)
        time.sleep(0.5)

    logging.warning('WARNING: (%s,"%s") not found within %f seconds', title,
                    class_name, timeout)
    return (None, None)


def ClickButton(button_hwnd):
    """Clicks a button window by sending a BM_CLICK message.

    Per http://msdn2.microsoft.com/en-us/library/bb775985.aspx
    "If the button is in a dialog box and the dialog box is not active, the
    BM_CLICK message might fail. To ensure success in this situation, call
    the SetActiveWindow function to activate the dialog box before sending
    the BM_CLICK message to the button."

    Args:
        button_hwnd: HWND to the button to be clicked.
    """
    previous_active_window = win32gui.SetActiveWindow(button_hwnd)
    win32gui.PostMessage(button_hwnd, win32con.BM_CLICK, 0, 0)
    if previous_active_window:
        win32gui.SetActiveWindow(previous_active_window)


def ClickChildButtonWithText(parent_hwnd, button_text):
    """Clicks a child button window with given text.

    Args:
        parent_hwnd: HWND of the button parent.
        button_text: Button windows title.

    Returns:
        Whether button is clicked.
    """
    button_hwnd = FindWindow(button_text, 'Button', parent_hwnd)
    if button_hwnd:
        logging.debug('Found child button with: %s', button_text)
        ClickButton(button_hwnd)
        return True

    logging.debug('No button with [%s] found.', button_text)
    return False


def SendKeyToWindow(hwnd, key_to_press):
    """Sends a key press to the window.

    This is a blocking call until all keys are pressed.

    Args:
        hwnd: handle of  the window to press key.
        key_to_press: Actual key to send to window. eg.'{Enter}'.
                      See `window_shell` documentation for key definitions.
    """
    try:
        window_shell = win32com.client.Dispatch('WScript.Shell')
        SetForegroundWindow(hwnd)
        window_shell.AppActivate(str(win32gui.GetForegroundWindow()))
        window_shell.SendKeys(key_to_press)
        logging.info('Sent %s to window %x.', key_to_press, hwnd)
    except pywintypes.error as err:
        logging.error('Failed to press key: %s', err)
        raise
    except pythoncom.com_error as err:
        logging.error('COM exception occurred: %s, is CoInitialize() called?',
                      err)
        raise
