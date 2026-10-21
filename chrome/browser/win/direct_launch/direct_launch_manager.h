// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_DIRECT_LAUNCH_DIRECT_LAUNCH_MANAGER_H_
#define CHROME_BROWSER_WIN_DIRECT_LAUNCH_DIRECT_LAUNCH_MANAGER_H_

#include <stdint.h>

#include <optional>
#include <string_view>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "base/win/windows_types.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/shell_integration.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace direct_launch {

// Registers the browser for direct launch with the Windows shell, and hands the
// launches the shell delivers to the browser. With direct launch, the shell
// hands URI launches (http and https) and Start menu tile launches straight to
// the running browser, instead of starting a new browser process that would
// only forward the launch to it through the process singleton.
//
// The registration is app-wide, and the shell only honors it while the browser
// is the default browser, so it is renewed whenever the default browser
// changes. Destroying the manager withdraws it, on a best effort basis: should
// the registration outlive the process, the shell falls back to the classic
// launch.
//
// Must be created, used and destroyed on the UI thread. The calls into the
// shell, which may load libraries and block, run on a ThreadPool sequence.
class DirectLaunchManager {
 public:
  // Subscribes the callback it is given to changes of the default browser,
  // like `default_browser::DefaultBrowserManager::
  // RegisterDefaultBrowserChanged()`. The callback is told whether this
  // browser is the default browser after the change.
  using DefaultBrowserChangeSubscriber =
      base::OnceCallback<base::CallbackListSubscription(
          base::RepeatingCallback<void(
              shell_integration::DefaultWebClientState)>)>;

  // Checks in the background whether this process can use direct launch, and
  // if so, registers for it, and renews the registration on the changes that
  // `subscriber` reports. Each launch the shell delivers is handed to
  // `launch_callback` as the command line of the classic launch that it
  // replaces, as if from another process.
  DirectLaunchManager(ProcessSingleton::NotificationCallback launch_callback,
                      DefaultBrowserChangeSubscriber subscriber);

  // As above, but calls into the shell on `shell_task_runner`.
  DirectLaunchManager(
      ProcessSingleton::NotificationCallback launch_callback,
      DefaultBrowserChangeSubscriber subscriber,
      scoped_refptr<base::SequencedTaskRunner> shell_task_runner);

  DirectLaunchManager(const DirectLaunchManager&) = delete;
  DirectLaunchManager& operator=(const DirectLaunchManager&) = delete;

  ~DirectLaunchManager();

  // Overrides whether the OS supports direct launch, or restores the real
  // answer when `is_supported` is nullopt.
  static void SetIsSupportedForTesting(std::optional<bool> is_supported);

 private:
  friend class DirectLaunchManagerTest;

  // Calls into the shell's direct launch API on the shell sequence. Defined in
  // the .cc file, to keep COM headers out of this one.
  class ShellRegistrar;

  // Finishes starting up once the background check for OS support is done.
  void OnSupportChecked(bool is_supported);

  // Registers for direct launch with the shell, replacing any previous
  // registration.
  void Register();

  // Renews the registration when this browser is, or might be, the default
  // browser after the change, since the shell only honors the registration
  // while this browser is the default browser.
  void OnDefaultBrowserChanged(shell_integration::DefaultWebClientState state);

  // Handles the messages that the shell delivers direct launches as.
  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  // Hands a direct launch of `uri`, or of the tile if `uri` is empty, to
  // `launch_callback_`.
  void Launch(std::wstring_view uri);

  const ProcessSingleton::NotificationCallback launch_callback_;

  // Run once the manager knows that this process uses direct launch, so that
  // a process that does not is never subscribed to default browser changes.
  DefaultBrowserChangeSubscriber subscriber_;

  // The copy data format id registered with the shell, echoed back by the
  // shell in `COPYDATASTRUCT::dwData`. Drawn at random, since
  // gfx::SingletonHwnd is shared with every other `WM_COPYDATA` sender.
  const uint32_t copy_data_format_id_;

  // The unique message id used for tile launch, or 0 when the manager has not
  // registered one, in which case tile launches are not registered with the
  // shell either. Compared against every message arriving on the shared
  // gfx::SingletonHwnd, so it must never start out as an arbitrary value.
  UINT tile_launch_message_id_ = 0;

  // Calls into the shell on the shell sequence. Null when this process does
  // not use direct launch.
  base::SequenceBound<ShellRegistrar> shell_registrar_;

  // Listens for messages from the shell.
  base::CallbackListSubscription hwnd_subscription_;

  // Renews the registration when the default browser changes.
  base::CallbackListSubscription default_browser_subscription_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DirectLaunchManager> weak_factory_{this};
};

}  // namespace direct_launch

#endif  // CHROME_BROWSER_WIN_DIRECT_LAUNCH_DIRECT_LAUNCH_MANAGER_H_
