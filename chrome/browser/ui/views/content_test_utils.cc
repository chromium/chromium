// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/content_test_utils.h"

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif  // defined(USE_AURA)

namespace {

// Helper to use inside a loop instead of using RunLoop::RunUntilIdle() to avoid
// the loop being a busy loop that prevents renderer from doing its job. Use
// only when there is no better way to synchronize.
void GiveItSomeTime(base::TimeDelta delta) {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), delta);
  run_loop.Run();
}

}  // namespace

void TestTextInputViaKeyEvent(content::WebContents* contents) {
  // Replace the dialog content with a single text input element and focus it.
  ASSERT_TRUE(content::WaitForLoadStop(contents));
  ASSERT_TRUE(content::ExecJs(contents, R"(
    document.body.innerHTML = trustedTypes.emptyHTML;
    const input = document.createElement('input');
    input.type = 'text';
    input.id = 'text-id';
    document.body.appendChild(input);
    document.getElementById('text-id').focus();
  )"));

  // Generate a key event for 'a'.
  gfx::NativeWindow event_window = contents->GetTopLevelNativeWindow();
#if defined(USE_AURA)
  event_window = event_window->GetRootWindow();
#endif
  ui::test::EventGenerator generator(event_window);
  generator.PressKey(ui::VKEY_A, ui::EF_NONE);

  // Verify text input is updated.
  std::string result;
  while (result != "a") {
    GiveItSomeTime(base::Milliseconds(100));

    result =
        content::EvalJs(contents, "document.getElementById('text-id').value;")
            .ExtractString();
  }
}
