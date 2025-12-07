// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/accessibility/ax_client/ax_client_ia2.h"

#include <oleacc.h>

#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/iaccessible2/ia2_api_all.h"

using Microsoft::WRL::ComPtr;

namespace ax_client {

namespace {

AxClientIa2* g_client = nullptr;

std::string EventToString(DWORD event) {
  switch (event) {
    // System-level events.
    case EVENT_SYSTEM_SOUND:  // 0x0001
      return "EVENT_SYSTEM_SOUND";
    case EVENT_SYSTEM_ALERT:  // 0x0002
      return "EVENT_SYSTEM_ALERT";
    case EVENT_SYSTEM_FOREGROUND:  // 0x0003
      return "EVENT_SYSTEM_FOREGROUND";
    case EVENT_SYSTEM_MENUSTART:  // 0x0004
      return "EVENT_SYSTEM_MENUSTART";
    case EVENT_SYSTEM_MENUEND:  // 0x0005
      return "EVENT_SYSTEM_MENUEND";
    case EVENT_SYSTEM_MENUPOPUPSTART:  // 0x0006
      return "EVENT_SYSTEM_MENUPOPUPSTART";
    case EVENT_SYSTEM_MENUPOPUPEND:  // 0x0007
      return "EVENT_SYSTEM_MENUPOPUPEND";
    case EVENT_SYSTEM_CAPTURESTART:  // 0x0008
      return "EVENT_SYSTEM_CAPTURESTART";
    case EVENT_SYSTEM_CAPTUREEND:  // 0x0009
      return "EVENT_SYSTEM_CAPTUREEND";
    case EVENT_SYSTEM_MOVESIZESTART:  // 0x000A
      return "EVENT_SYSTEM_MOVESIZESTART";
    case EVENT_SYSTEM_MOVESIZEEND:  // 0x000B
      return "EVENT_SYSTEM_MOVESIZEEND";
    case EVENT_SYSTEM_CONTEXTHELPSTART:  // 0x000C
      return "EVENT_SYSTEM_CONTEXTHELPSTART";
    case EVENT_SYSTEM_CONTEXTHELPEND:  // 0x000D
      return "EVENT_SYSTEM_CONTEXTHELPEND";
    case EVENT_SYSTEM_DRAGDROPSTART:  // 0x000E
      return "EVENT_SYSTEM_DRAGDROPSTART";
    case EVENT_SYSTEM_DRAGDROPEND:  // 0x000F
      return "EVENT_SYSTEM_DRAGDROPEND";
    case EVENT_SYSTEM_DIALOGSTART:  // 0x0010
      return "EVENT_SYSTEM_DIALOGSTART";
    case EVENT_SYSTEM_DIALOGEND:  // 0x0011
      return "EVENT_SYSTEM_DIALOGEND";
    case EVENT_SYSTEM_SCROLLINGSTART:  // 0x0012
      return "EVENT_SYSTEM_SCROLLINGSTART";
    case EVENT_SYSTEM_SCROLLINGEND:  // 0x0013
      return "EVENT_SYSTEM_SCROLLINGEND";
    case EVENT_SYSTEM_SWITCHSTART:  // 0x0014
      return "EVENT_SYSTEM_SWITCHSTART";
    case EVENT_SYSTEM_SWITCHEND:  // 0x0015
      return "EVENT_SYSTEM_SWITCHEND";
    case EVENT_SYSTEM_MINIMIZESTART:  // 0x0016
      return "EVENT_SYSTEM_MINIMIZESTART";
    case EVENT_SYSTEM_MINIMIZEEND:  // 0x0017
      return "EVENT_SYSTEM_MINIMIZEEND";
    case EVENT_SYSTEM_DESKTOPSWITCH:  // 0x0020
      return "EVENT_SYSTEM_DESKTOPSWITCH";
    case EVENT_SYSTEM_SWITCHER_APPGRABBED:  // 0x0024
      return "EVENT_SYSTEM_SWITCHER_APPGRABBED";
    case EVENT_SYSTEM_SWITCHER_APPOVERTARGET:  // 0x0025
      return "EVENT_SYSTEM_SWITCHER_APPOVERTARGET";
    case EVENT_SYSTEM_SWITCHER_APPDROPPED:  // 0x0026
      return "EVENT_SYSTEM_SWITCHER_APPDROPPED";
    case EVENT_SYSTEM_SWITCHER_CANCELLED:  // 0x0027
      return "EVENT_SYSTEM_SWITCHER_CANCELLED";
    case EVENT_SYSTEM_IME_KEY_NOTIFICATION:  // 0x0029
      return "EVENT_SYSTEM_IME_KEY_NOTIFICATION";
    case EVENT_SYSTEM_END:  // 0x00FF
      return "EVENT_SYSTEM_END";

    // IAccessible2 event IDs
    case IA2_EVENT_ACTION_CHANGED:  // 0x0101
      return "IA2_EVENT_ACTION_CHANGED";
    case IA2_EVENT_ACTIVE_DECENDENT_CHANGED:  // 0x0102
      return "IA2_EVENT_ACTIVE_DECENDENT_CHANGED";
    case IA2_EVENT_DOCUMENT_ATTRIBUTE_CHANGED:  // 0x0103
      return "IA2_EVENT_DOCUMENT_ATTRIBUTE_CHANGED";
    case IA2_EVENT_DOCUMENT_CONTENT_CHANGED:  // 0x0104
      return "IA2_EVENT_DOCUMENT_CONTENT_CHANGED";
    case IA2_EVENT_DOCUMENT_LOAD_COMPLETE:  // 0x0105
      return "IA2_EVENT_DOCUMENT_LOAD_COMPLETE";
    case IA2_EVENT_DOCUMENT_LOAD_STOPPED:  // 0x0106
      return "IA2_EVENT_DOCUMENT_LOAD_STOPPED";
    case IA2_EVENT_DOCUMENT_RELOAD:  // 0x0107
      return "IA2_EVENT_DOCUMENT_RELOAD";
    case IA2_EVENT_HYPERLINK_END_INDEX_CHANGED:  // 0x0108
      return "IA2_EVENT_HYPERLINK_END_INDEX_CHANGED";
    case IA2_EVENT_HYPERLINK_NUMBER_OF_ANCHORS_CHANGED:  // 0x0109
      return "IA2_EVENT_HYPERLINK_NUMBER_OF_ANCHORS_CHANGED";
    case IA2_EVENT_HYPERLINK_SELECTED_LINK_CHANGED:  // 0x010A
      return "IA2_EVENT_HYPERLINK_SELECTED_LINK_CHANGED";
    case IA2_EVENT_HYPERTEXT_LINK_ACTIVATED:  // 0x010B
      return "IA2_EVENT_HYPERTEXT_LINK_ACTIVATED";
    case IA2_EVENT_HYPERTEXT_LINK_SELECTED:  // 0x010C
      return "IA2_EVENT_HYPERTEXT_LINK_SELECTED";
    case IA2_EVENT_HYPERLINK_START_INDEX_CHANGED:  // 0x010D
      return "IA2_EVENT_HYPERLINK_START_INDEX_CHANGED";
    case IA2_EVENT_HYPERTEXT_CHANGED:  // 0x010E
      return "IA2_EVENT_HYPERTEXT_CHANGED";
    case IA2_EVENT_HYPERTEXT_NLINKS_CHANGED:  // 0x010F
      return "IA2_EVENT_HYPERTEXT_NLINKS_CHANGED";
    case IA2_EVENT_OBJECT_ATTRIBUTE_CHANGED:  // 0x0110
      return "IA2_EVENT_OBJECT_ATTRIBUTE_CHANGED";
    case IA2_EVENT_PAGE_CHANGED:  // 0x0111
      return "IA2_EVENT_PAGE_CHANGED";
    case IA2_EVENT_SECTION_CHANGED:  // 0x0112
      return "IA2_EVENT_SECTION_CHANGED";
    case IA2_EVENT_TABLE_CAPTION_CHANGED:  // 0x0113
      return "IA2_EVENT_TABLE_CAPTION_CHANGED";
    case IA2_EVENT_TABLE_COLUMN_DESCRIPTION_CHANGED:  // 0x0114
      return "IA2_EVENT_TABLE_COLUMN_DESCRIPTION_CHANGED";
    case IA2_EVENT_TABLE_COLUMN_HEADER_CHANGED:  // 0x0115
      return "IA2_EVENT_TABLE_COLUMN_HEADER_CHANGED";
    case IA2_EVENT_TABLE_MODEL_CHANGED:  // 0x0116
      return "IA2_EVENT_TABLE_MODEL_CHANGED";
    case IA2_EVENT_TABLE_ROW_DESCRIPTION_CHANGED:  // 0x0117
      return "IA2_EVENT_TABLE_ROW_DESCRIPTION_CHANGED";
    case IA2_EVENT_TABLE_ROW_HEADER_CHANGED:  // 0x0118
      return "IA2_EVENT_TABLE_ROW_HEADER_CHANGED";
    case IA2_EVENT_TABLE_SUMMARY_CHANGED:  // 0x0119
      return "IA2_EVENT_TABLE_SUMMARY_CHANGED";
    case IA2_EVENT_TEXT_ATTRIBUTE_CHANGED:  // 0x011A
      return "IA2_EVENT_TEXT_ATTRIBUTE_CHANGED";
    case IA2_EVENT_TEXT_CARET_MOVED:  // 0x011B
      return "IA2_EVENT_TEXT_CARET_MOVED";
    case IA2_EVENT_TEXT_CHANGED:  // 0x011C
      return "IA2_EVENT_TEXT_CHANGED";
    case IA2_EVENT_TEXT_COLUMN_CHANGED:  // 0x011D
      return "IA2_EVENT_TEXT_COLUMN_CHANGED";
    case IA2_EVENT_TEXT_INSERTED:  // 0x011E
      return "IA2_EVENT_TEXT_INSERTED";
    case IA2_EVENT_TEXT_REMOVED:  // 0x011F
      return "IA2_EVENT_TEXT_REMOVED";
    case IA2_EVENT_TEXT_UPDATED:  // 0x0120
      return "IA2_EVENT_TEXT_UPDATED";
    case IA2_EVENT_TEXT_SELECTION_CHANGED:  // 0x0121
      return "IA2_EVENT_TEXT_SELECTION_CHANGED";
    case IA2_EVENT_VISIBLE_DATA_CHANGED:  // 0x0122
      return "IA2_EVENT_VISIBLE_DATA_CHANGED";
    case IA2_EVENT_ROLE_CHANGED:  // 0x0123
      return "IA2_EVENT_ROLE_CHANGED";

    // OEM reserved events.
    case EVENT_OEM_DEFINED_END:  // 0x01FF
      return "EVENT_OEM_DEFINED_END";

    // Windows console-specific events
    case EVENT_CONSOLE_CARET:  // 0x4001
      return "EVENT_CONSOLE_CARET";
    case EVENT_CONSOLE_UPDATE_REGION:  // 0x4002
      return "EVENT_CONSOLE_UPDATE_REGION";
    case EVENT_CONSOLE_UPDATE_SIMPLE:  // 0x4003
      return "EVENT_CONSOLE_UPDATE_SIMPLE";
    case EVENT_CONSOLE_UPDATE_SCROLL:  // 0x4004
      return "EVENT_CONSOLE_UPDATE_SCROLL";
    case EVENT_CONSOLE_LAYOUT:  // 0x4005
      return "EVENT_CONSOLE_LAYOUT";
    case EVENT_CONSOLE_START_APPLICATION:  // 0x4006
      return "EVENT_CONSOLE_START_APPLICATION";
    case EVENT_CONSOLE_END_APPLICATION:  // 0x4007
      return "EVENT_CONSOLE_END_APPLICATION";
    case EVENT_CONSOLE_END:  // 0x40FF
      return "EVENT_CONSOLE_END";

    // UI Automation events.
    case EVENT_UIA_EVENTID_START:  // 0x4E00
      return "EVENT_UIA_EVENTID_START";
    case EVENT_UIA_EVENTID_END:  // 0x4EFF
      return "EVENT_UIA_EVENTID_END";
    case EVENT_UIA_PROPID_START:  // 0x7500
      return "EVENT_UIA_PROPID_START";
    case EVENT_UIA_PROPID_END:  // 0x75FF
      return "EVENT_UIA_PROPID_END";

    // Object-level events.
    case EVENT_OBJECT_CREATE:  // 0x8000
      return "EVENT_OBJECT_CREATE";
    case EVENT_OBJECT_DESTROY:  // 0x8001
      return "EVENT_OBJECT_DESTROY";
    case EVENT_OBJECT_SHOW:  // 0x8002
      return "EVENT_OBJECT_SHOW";
    case EVENT_OBJECT_HIDE:  // 0x8003
      return "EVENT_OBJECT_HIDE";
    case EVENT_OBJECT_REORDER:  // 0x8004
      return "EVENT_OBJECT_REORDER";
    case EVENT_OBJECT_FOCUS:  // 0x8005
      return "EVENT_OBJECT_FOCUS";
    case EVENT_OBJECT_SELECTION:  // 0x8006
      return "EVENT_OBJECT_SELECTION";
    case EVENT_OBJECT_SELECTIONADD:  // 0x8007
      return "EVENT_OBJECT_SELECTIONADD";
    case EVENT_OBJECT_SELECTIONREMOVE:  // 0x8008
      return "EVENT_OBJECT_SELECTIONREMOVE";
    case EVENT_OBJECT_SELECTIONWITHIN:  // 0x8009
      return "EVENT_OBJECT_SELECTIONWITHIN";
    case EVENT_OBJECT_STATECHANGE:  // 0x800A
      return "EVENT_OBJECT_STATECHANGE";
    case EVENT_OBJECT_LOCATIONCHANGE:  // 0x800B
      return "EVENT_OBJECT_LOCATIONCHANGE";
    case EVENT_OBJECT_NAMECHANGE:  // 0x800C
      return "EVENT_OBJECT_NAMECHANGE";
    case EVENT_OBJECT_DESCRIPTIONCHANGE:  // 0x800D
      return "EVENT_OBJECT_DESCRIPTIONCHANGE";
    case EVENT_OBJECT_VALUECHANGE:  // 0x800E
      return "EVENT_OBJECT_VALUECHANGE";
    case EVENT_OBJECT_PARENTCHANGE:  // 0x800F
      return "EVENT_OBJECT_PARENTCHANGE";
    case EVENT_OBJECT_HELPCHANGE:  // 0x8010
      return "EVENT_OBJECT_HELPCHANGE";
    case EVENT_OBJECT_DEFACTIONCHANGE:  // 0x8011
      return "EVENT_OBJECT_DEFACTIONCHANGE";
    case EVENT_OBJECT_ACCELERATORCHANGE:  // 0x8012
      return "EVENT_OBJECT_ACCELERATORCHANGE";
    case EVENT_OBJECT_INVOKED:  // 0x8013
      return "EVENT_OBJECT_INVOKED";
    case EVENT_OBJECT_TEXTSELECTIONCHANGED:  // 0x8014
      return "EVENT_OBJECT_TEXTSELECTIONCHANGED";
    case EVENT_OBJECT_CONTENTSCROLLED:  // 0x8015
      return "EVENT_OBJECT_CONTENTSCROLLED";
    case EVENT_SYSTEM_ARRANGMENTPREVIEW:  // 0x8016
      return "EVENT_SYSTEM_ARRANGMENTPREVIEW";
    case EVENT_OBJECT_CLOAKED:  // 0x8017
      return "EVENT_OBJECT_CLOAKED";
    case EVENT_OBJECT_UNCLOAKED:  // 0x8018
      return "EVENT_OBJECT_UNCLOAKED";
    case EVENT_OBJECT_LIVEREGIONCHANGED:  // 0x8019
      return "EVENT_OBJECT_LIVEREGIONCHANGED";
    case EVENT_OBJECT_HOSTEDOBJECTSINVALIDATED:  // 0x8020
      return "EVENT_OBJECT_HOSTEDOBJECTSINVALIDATED";
    case EVENT_OBJECT_DRAGSTART:  // 0x8021
      return "EVENT_OBJECT_DRAGSTART";
    case EVENT_OBJECT_DRAGCANCEL:  // 0x8022
      return "EVENT_OBJECT_DRAGCANCEL";
    case EVENT_OBJECT_DRAGCOMPLETE:  // 0x8023
      return "EVENT_OBJECT_DRAGCOMPLETE";
    case EVENT_OBJECT_DRAGENTER:  // 0x8024
      return "EVENT_OBJECT_DRAGENTER";
    case EVENT_OBJECT_DRAGLEAVE:  // 0x8025
      return "EVENT_OBJECT_DRAGLEAVE";
    case EVENT_OBJECT_DRAGDROPPED:  // 0x8026
      return "EVENT_OBJECT_DRAGDROPPED";
    case EVENT_OBJECT_IME_SHOW:  // 0x8027
      return "EVENT_OBJECT_IME_SHOW";
    case EVENT_OBJECT_IME_HIDE:  // 0x8028
      return "EVENT_OBJECT_IME_HIDE";
    case EVENT_OBJECT_IME_CHANGE:  // 0x8029
      return "EVENT_OBJECT_IME_CHANGE";
    case EVENT_OBJECT_TEXTEDIT_CONVERSIONTARGETCHANGED:  // 0x8030
      return "EVENT_OBJECT_TEXTEDIT_CONVERSIONTARGETCHANGED";
    case EVENT_OBJECT_END:  // 0x80FF
      return "EVENT_OBJECT_END";

    // Accessibility Interoperability Alliance events.
    case EVENT_AIA_START:  // 0xA000
      return "EVENT_AIA_START";
    case EVENT_AIA_END:  // 0xAFFF
      return "EVENT_AIA_END";

    default:
      break;
  }

  return base::StringPrintf("0x%04X", event);
}

std::string ObjIdToString(LONG id_object) {
  switch (id_object) {
    case OBJID_WINDOW:  // 0x00000000
      return "OBJID_WINDOW";
    case OBJID_SYSMENU:  // 0xFFFFFFFF
      return "OBJID_SYSMENU";
    case OBJID_TITLEBAR:  // 0xFFFFFFFE
      return "OBJID_TITLEBAR";
    case OBJID_MENU:  // 0xFFFFFFFD
      return "OBJID_MENU";
    case OBJID_CLIENT:  // 0xFFFFFFFC
      return "OBJID_CLIENT";
    case OBJID_VSCROLL:  // 0xFFFFFFFB
      return "OBJID_VSCROLL";
    case OBJID_HSCROLL:  // 0xFFFFFFFA
      return "OBJID_HSCROLL";
    case OBJID_SIZEGRIP:  // 0xFFFFFFF9
      return "OBJID_SIZEGRIP";
    case OBJID_CARET:  // 0xFFFFFFF8
      return "OBJID_CARET";
    case OBJID_CURSOR:  // 0xFFFFFFF7
      return "OBJID_CURSOR";
    case OBJID_ALERT:  // 0xFFFFFFF6
      return "OBJID_ALERT";
    case OBJID_SOUND:  // 0xFFFFFFF5
      return "OBJID_SOUND";
    case OBJID_QUERYCLASSNAMEIDX:  // 0xFFFFFFF4
      return "OBJID_QUERYCLASSNAMEIDX";
    case OBJID_NATIVEOM:  // 0xFFFFFFF0
      return "OBJID_NATIVEOM";
  }

  return base::StringPrintf("0x%04X", id_object);
}

std::string ChildIdToString(LONG id_child) {
  if (id_child == CHILDID_SELF) {
    return "CHILDID_SELF";
  }

  return base::StringPrintf("0x%04X", id_child);
}

}  // namespace

// static
bool EventHookHandleTraits::CloseHandle(Handle handle) {
  return ::UnhookWinEvent(handle) != FALSE;
}

AxClientIa2::AxClientIa2() {
  CHECK_EQ(std::exchange(g_client, this), nullptr);
}

AxClientIa2::~AxClientIa2() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK_EQ(std::exchange(g_client, nullptr), this);
}

HRESULT AxClientIa2::Initialize(HWND hwnd) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  HRESULT hr = E_FAIL;

  DWORD remote_thread_id =
      ::GetWindowThreadProcessId(hwnd, /*lpdwProcessId=*/nullptr);
  if (!remote_thread_id) {
    const auto error = ::GetLastError();
    PLOG(ERROR) << __func__ << " Failed to get remote TID";
    return HRESULT_FROM_WIN32(error);
  }

  ComPtr<IAccessible> main_window_acc;
  hr = ::AccessibleObjectFromWindow(hwnd, /*dwId=*/OBJID_CLIENT,
                                    IID_PPV_ARGS(&main_window_acc));
  if (FAILED(hr)) {
    LOG(ERROR) << __func__ << " Failed to get IAccessible for remote window "
               << logging::SystemErrorCodeToString(hr);
    return hr;
  }

  VLOG(1) << __func__ << " Got object for target window with hwnd: " << std::hex
          << hwnd;

  // Register for events from the thread hosting the remote window.
  ScopedEventHookHandle event_hook_handle(::SetWinEventHook(
      /*eventMin=*/EVENT_MIN,
      /*eventMax=*/EVENT_MAX,
      /*hmodWinEventProc=*/nullptr,
      /*pfnWinEventProc=*/&AxClientIa2::OnWinEvent,
      /*idProcess=*/0,
      /*idThread=*/remote_thread_id,
      /*dwFlags=*/WINEVENT_OUTOFCONTEXT));
  if (!event_hook_handle.is_valid()) {
    const auto error = ::GetLastError();
    PLOG(ERROR) << __func__ << " Failed to install WinEvent hook";
    return HRESULT_FROM_WIN32(error);
  }

  main_window_ = hwnd;
  main_window_acc_ = std::move(main_window_acc);
  event_hook_handle_ = std::move(event_hook_handle);

  return S_OK;
}

HRESULT AxClientIa2::FindAll() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(a11y): Use IA2 APIs to discover all accessible elements under
  // main_window_acc_.

  return S_OK;
}

void AxClientIa2::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Unregister the WinEvent hook.
  event_hook_handle_.Close();

  // Release all references.
  main_window_ = 0;
  main_window_acc_.Reset();
}

// static
void CALLBACK AxClientIa2::OnWinEvent(HWINEVENTHOOK win_event_hook,
                                      DWORD event,
                                      HWND hwnd,
                                      LONG id_object,
                                      LONG id_child,
                                      DWORD id_event_thread,
                                      DWORD event_time_ms) {
  CHECK(g_client);
  DCHECK_CALLED_ON_VALID_SEQUENCE(g_client->sequence_checker_);

  VLOG(1) << __func__ << " event: " << EventToString(event)
          << " hwnd: " << std::hex << hwnd
          << " id_object: " << ObjIdToString(id_object)
          << " id_child: " << ChildIdToString(id_child);

  if (event == EVENT_OBJECT_DESTROY) {
    g_client->OnObjectDestroy(hwnd, id_object, id_child);
  }
}

void AxClientIa2::OnObjectDestroy(HWND hwnd, LONG id_object, LONG id_child) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (hwnd == main_window_ && id_object == OBJID_WINDOW &&
      id_child == CHILDID_SELF) {
    VLOG(1) << __func__
            << " Releasing all elements for the main browser window";
    // The top-level browser window itself has been removed. Forget about it and
    // all discovered elements.
    main_window_ = 0;
    CHECK_EQ(main_window_acc_.Reset(), 0U);
  }
}

}  // namespace ax_client
