// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_TRAY_STATE_CHANGER_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_TRAY_STATE_CHANGER_WIN_H_

#include <wrl/client.h>
#include <wrl/implements.h>

#include <memory>
#include <string>

#include "base/threading/thread_checker.h"

// The known values for NOTIFYITEM's dwPreference member.
enum NOTIFYITEM_PREFERENCE {
  // In Windows UI: "Only show notifications."
  PREFERENCE_SHOW_WHEN_ACTIVE = 0,
  // In Windows UI: "Hide icon and notifications."
  PREFERENCE_SHOW_NEVER = 1,
  // In Windows UI: "Show icon and notifications."
  PREFERENCE_SHOW_ALWAYS = 2
};

// NOTIFYITEM describes an entry in Explorer's registry of status icons.
// Explorer keeps entries around for a process even after it exits.
struct NOTIFYITEM {
  PWSTR exe_name;    // The file name of the creating executable.
  PWSTR tip;         // The last hover-text value associated with this status
                     // item.
  HICON icon;        // The icon associated with this status item.
  HWND hwnd;         // The HWND associated with the status item.
  DWORD preference;  // Determines the behavior of the icon with respect to
                     // the taskbar. Values taken from NOTIFYITEM_PREFERENCE.
  UINT id;           // The ID specified by the application.  (hWnd, uID) is
                     // unique.
  GUID guid;         // The GUID specified by the application, alternative to
                     // uID.
};

// INotificationCB is an interface that applications can implement in order to
// receive notifications about the state of the notification area manager.
class __declspec(uuid("D782CCBA-AFB0-43F1-94DB-FDA3779EACCB")) INotificationCB
    : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE
      Notify(ULONG event, NOTIFYITEM* notify_item) = 0;
};

// A class that is capable of reading and writing the state of the notification
// area in the Windows taskbar.  It is used to promote a tray icon from the
// overflow area to the taskbar, and refuses to do anything if the user has
// explicitly marked an icon to be always hidden.
class StatusTrayStateChangerWin
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          INotificationCB> {
 public:
  StatusTrayStateChangerWin(UINT icon_id, HWND window);

  StatusTrayStateChangerWin(const StatusTrayStateChangerWin&) = delete;
  StatusTrayStateChangerWin& operator=(const StatusTrayStateChangerWin&) =
      delete;

  // Call this method to move the icon matching |icon_id| and |window| to the
  // taskbar from the overflow area.  This will not make any changes if the
  // icon has been set to |PREFERENCE_SHOW_NEVER|, in order to comply with
  // the explicit wishes/configuration of the user.
  void EnsureTrayIconVisible();

  // INotificationCB.
  // Notify is called in response to RegisterCallback for each current
  // entry in Explorer's list of notification area icons, and ever time
  // one of them changes, until UnregisterCallback is called or |this|
  // is destroyed.
  IFACEMETHODIMP Notify(ULONG, NOTIFYITEM*) override;

 protected:
  ~StatusTrayStateChangerWin() override;

 private:
  friend class StatusTrayStateChangerWinTest;

  enum InterfaceVersion {
    INTERFACE_VERSION_LEGACY = 0,
    INTERFACE_VERSION_WIN8,
    INTERFACE_VERSION_UNKNOWN
  };

  // Creates an instance of TrayNotify, and ensures that it supports either
  // ITrayNotify or ITrayNotifyWin8.  Returns true on success.
  bool CreateTrayNotify();

  // Returns the NOTIFYITEM that corresponds to this executable and the
  // HWND/ID pair that were used to create the StatusTrayStateChangerWin.
  // Internally it calls the appropriate RegisterCallback{Win8,Legacy}.
  std::unique_ptr<NOTIFYITEM> RegisterCallback();

  // Calls RegisterCallback with the appropriate interface required by
  // different versions of Windows.  This will result in |notify_item_| being
  // updated when a matching item is passed into
  // StatusTrayStateChangerWin::Notify.
  bool RegisterCallbackWin8();
  bool RegisterCallbackLegacy();

  // Sends an update to Explorer with the passed NOTIFYITEM.
  void SendNotifyItemUpdate(std::unique_ptr<NOTIFYITEM> notify_item);

  // Storing IUnknown since we will need to use different interfaces
  // for different versions of Windows.
  Microsoft::WRL::ComPtr<IUnknown> tray_notify_;
  InterfaceVersion interface_version_;

  // The ID assigned to the notification area icon that we want to manipulate.
  const UINT icon_id_;
  // The HWND associated with the notification area icon that we want to
  // manipulate.  This is an unretained pointer, do not dereference.
  const HWND window_;
  // Executable name of the current program.  Along with |icon_id_| and
  // |window_|, this uniquely identifies a notification area entry to Explorer.
  std::wstring file_name_;

  // Temporary storage for the matched NOTIFYITEM.  This is necessary because
  // Notify doesn't return anything.  The call flow looks like this:
  //   TrayNotify->RegisterCallback()
  //      ... other COM stack frames ..
  //   StatusTrayStateChangerWin->Notify(NOTIFYITEM);
  // so we can't just return the notifyitem we're looking for.
  std::unique_ptr<NOTIFYITEM> notify_item_;

  THREAD_CHECKER(thread_checker_);
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_TRAY_STATE_CHANGER_WIN_H_
