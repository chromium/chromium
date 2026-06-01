// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shell_delegate/chrome_shell_delegate.h"

#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "ash/shell.h"
#include "ash/wm/window_pin_util.h"
#include "ash/wm/window_state.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/base/base_window.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/models/image_model.h"

namespace ash {

using ChromeShellDelegateBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(ChromeShellDelegateBrowserTest,
                       LockedFullscreenClearsClipboard) {
  // 1. Write HTML content to the clipboard.
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteHTML(u"<img>test</img>", "source_url");
  }

  // Verify clipboard has content.
  ui::ClipboardNonBacked* clipboard =
      ui::ClipboardNonBacked::GetForCurrentThread();
  ASSERT_TRUE(clipboard);
  EXPECT_NE(nullptr, clipboard->GetClipboardData(nullptr));

  // 2. Start a rendering request in ClipboardImageModelFactory.
  ClipboardImageModelFactory* factory = ClipboardImageModelFactory::Get();
  ASSERT_TRUE(factory);
  factory->Activate();
  base::UnguessableToken token = base::UnguessableToken::Create();
  bool callback_ran = false;
  factory->Render(
      token, "<img>test</img>", gfx::Size(100, 100),
      base::BindOnce([](bool* ran, ui::ImageModel model) { *ran = true; },
                     &callback_ran));

  // 3. Transition to Locked Fullscreen (Pin window).
  aura::Window* window = GlobalBrowserCollection::GetInstance()
                             ->GetLastActiveBrowser()
                             ->GetWindow()
                             ->GetNativeWindow();
  WindowState* window_state = WindowState::Get(window);
  ASSERT_TRUE(window_state);
  PinWindow(window, /*trusted=*/true);
  EXPECT_TRUE(window_state->IsPinned());
  // Transitioning to locked fullscreen triggers ToggleLockedFullscreen on the
  // WindowStateDelegate, which handles environment setup.

  // Pump active tasks just past the 250ms rendering debounce timer.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(300));
  run_loop.Run();

  // 4. Verify that the render was cancelled and clipboard wiped.
  EXPECT_FALSE(callback_ran)
      << "In-flight render was not cancelled on lockdown!";
  EXPECT_EQ(nullptr, clipboard->GetClipboardData(nullptr));
}

}  // namespace ash
