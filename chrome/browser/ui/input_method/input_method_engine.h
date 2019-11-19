// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INPUT_METHOD_INPUT_METHOD_ENGINE_H_
#define CHROME_BROWSER_UI_INPUT_METHOD_INPUT_METHOD_ENGINE_H_

#include <string>
#include <vector>

#include "chrome/browser/ui/input_method/ime_window.h"
#include "chrome/browser/ui/input_method/ime_window_observer.h"
#include "chrome/browser/ui/input_method/input_method_engine_base.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace input_method {

class InputMethodEngine : public InputMethodEngineBase,
                          public ui::ImeWindowObserver {
 public:
  InputMethodEngine();

  ~InputMethodEngine() override;

  // input_method::InputMethodEngineBase overrides:
  void FocusOut() override;
  void SetCompositionBounds(const std::vector<gfx::Rect>& bounds) override;
  void UpdateComposition(const ui::CompositionText& composition_text,
                         uint32_t cursor_pos,
                         bool is_visible) override;
  bool SetCompositionRange(
      uint32_t before,
      uint32_t after,
      const std::vector<ui::ImeTextSpan>& text_spans) override;

  bool SetSelectionRange(uint32_t start, uint32_t end) override;

  void CommitTextToInputContext(int context_id,
                                const std::string& text) override;
  bool SendKeyEvent(ui::KeyEvent* ui_event, const std::string& code) override;
  bool IsActive() const override;

  std::string GetExtensionId() const;

  // Creates and shows the IME window.
  // Returns 0 for errors and |error| will contains the error message.
  int CreateImeWindow(const extensions::Extension* extension,
                      content::RenderFrameHost* render_frame_host,
                      const std::string& url,
                      ui::ImeWindow::Mode mode,
                      const gfx::Rect& bounds,
                      std::string* error);
  void ShowImeWindow(int window_id);
  void HideImeWindow(int window_id);
  void CloseImeWindows();

 private:
  // ui::ImeWindowObserver:
  void OnWindowDestroyed(ui::ImeWindow* ime_window) override;

  ui::ImeWindow* FindWindowById(int window_id) const;

  // Checks if the page is special page that we want to disable some key events.
  bool IsSpecialPage(ui::InputMethod* method);

  // Checks if the key event are whitelisted key for all pages.
  bool IsValidKeyForAllPages(ui::KeyEvent* ui_event);

  // Holds the IME window instances for properly closing in the destructor.
  // The follow-cursor window is singleton.
  // The normal windows cannot exceed the max count.
  ui::ImeWindow* follow_cursor_window_;         // Weak.
  std::vector<ui::ImeWindow*> normal_windows_;  // Weak.

  // Tracks the current cursor bounds so that the new follow cursor window can
  // be aligned with cursor once it being created.
  gfx::Rect current_cursor_bounds_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodEngine);
};

}  // namespace input_method

#endif  // CHROME_BROWSER_UI_INPUT_METHOD_INPUT_METHOD_ENGINE_H_
