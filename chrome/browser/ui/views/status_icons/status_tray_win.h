// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_TRAY_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_TRAY_WIN_H_

#include <windows.h>

#include <memory>

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/status_icons/status_tray.h"

namespace base {
class SequencedTaskRunner;
}

class StatusIconWin;

// A class that's responsible for increasing, if possible, the visibility
// of a status tray icon on the taskbar. The default implementation sends
// a task to a worker thread each time EnqueueChange is called.
class StatusTrayStateChangerProxy {
 public:
  virtual ~StatusTrayStateChangerProxy() = default;

  // Called by StatusTrayWin to request upgraded visibility on the icon
  // represented by the |icon_id|, |window| pair.
  virtual void EnqueueChange(UINT icon_id, HWND window) = 0;
};

class StatusTrayWin : public StatusTray {
 public:
  // Keeps the notification HWND alive while Shell requests are in flight.
  // The HWND must be destroyed on its creating (UI) sequence.
  class Window : public base::RefCountedDeleteOnSequence<Window> {
   public:
    explicit Window(StatusTrayWin* tray);

    HWND hwnd() const { return hwnd_; }
    const scoped_refptr<base::SequencedTaskRunner>& task_runner() const {
      return task_runner_;
    }
    void Detach();

   private:
    friend class base::RefCountedDeleteOnSequence<Window>;
    friend class base::DeleteHelper<Window>;
    ~Window();

    class MessageWindow;
    const std::unique_ptr<MessageWindow> message_window_;
    const HWND hwnd_;
    const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  };

  StatusTrayWin();

  StatusTrayWin(const StatusTrayWin&) = delete;
  StatusTrayWin& operator=(const StatusTrayWin&) = delete;

  ~StatusTrayWin() override;

  void UpdateIconVisibilityInBackground(StatusIconWin* status_icon);

  void SetStatusTrayStateChangerProxyForTest(
      std::unique_ptr<StatusTrayStateChangerProxy> proxy);

  // Exposed for testing.
  LRESULT CALLBACK WndProc(HWND hwnd,
                           UINT message,
                           WPARAM wparam,
                           LPARAM lparam);

 protected:
  // Overriden from StatusTray:
  std::unique_ptr<StatusIcon> CreatePlatformStatusIcon(
      StatusIconType type,
      const gfx::ImageSkia& image,
      const std::u16string& tool_tip) override;

 private:
  UINT NextIconId();

  // The unique icon ID we will assign to the next icon.
  UINT next_icon_id_;

  // Shared with icons and outstanding Shell requests.
  scoped_refptr<Window> window_;

  // The message ID of the "TaskbarCreated" message, sent to us when we need to
  // reset our status icons.
  UINT taskbar_created_message_;

  // Manages changes performed on a background thread to manipulate visibility
  // of notification icons.
  std::unique_ptr<StatusTrayStateChangerProxy> state_changer_proxy_;

  base::WeakPtrFactory<StatusTrayWin> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_TRAY_WIN_H_
