// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IME_DRIVER_REMOTE_TEXT_INPUT_CLIENT_H_
#define CHROME_BROWSER_UI_VIEWS_IME_DRIVER_REMOTE_TEXT_INPUT_CLIENT_H_

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "services/ws/public/mojom/ime/ime.mojom.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/text_input_client.h"

// This implementation of ui::TextInputClient sends all updates via mojo IPC to
// a remote client. This is intended to be passed to the overrides of
// ui::InputMethod::SetFocusedTextInputClient().
class RemoteTextInputClient : public ui::TextInputClient,
                              public ui::internal::InputMethodDelegate {
 public:
  RemoteTextInputClient(ws::mojom::TextInputClientPtr remote_client,
                        ui::TextInputType text_input_type,
                        ui::TextInputMode text_input_mode,
                        base::i18n::TextDirection text_direction,
                        int text_input_flags,
                        gfx::Rect caret_bounds);
  ~RemoteTextInputClient() override;

  void SetTextInputType(ui::TextInputType text_input_type);
  void SetCaretBounds(const gfx::Rect& caret_bounds);

 private:
  // See |pending_callbacks_| for details.
  void OnDispatchKeyEventPostIMECompleted(bool completed);

  // ui::TextInputClient:
  void SetCompositionText(const ui::CompositionText& composition) override;
  void ConfirmCompositionText() override;
  void ClearCompositionText() override;
  void InsertText(const base::string16& text) override;
  void InsertChar(const ui::KeyEvent& event) override;
  ui::TextInputType GetTextInputType() const override;
  ui::TextInputMode GetTextInputMode() const override;
  base::i18n::TextDirection GetTextDirection() const override;
  int GetTextInputFlags() const override;
  bool CanComposeInline() const override;
  gfx::Rect GetCaretBounds() const override;
  bool GetCompositionCharacterBounds(uint32_t index,
                                     gfx::Rect* rect) const override;
  bool HasCompositionText() const override;
  FocusReason GetFocusReason() const override;
  bool GetTextRange(gfx::Range* range) const override;
  bool GetCompositionTextRange(gfx::Range* range) const override;
  bool GetSelectionRange(gfx::Range* range) const override;
  bool SetSelectionRange(const gfx::Range& range) override;
  bool DeleteRange(const gfx::Range& range) override;
  bool GetTextFromRange(const gfx::Range& range,
                        base::string16* text) const override;
  void OnInputMethodChanged() override;
  bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override;
  void ExtendSelectionAndDelete(size_t before, size_t after) override;
  void EnsureCaretNotInRect(const gfx::Rect& rect) override;
  bool IsTextEditCommandEnabled(ui::TextEditCommand command) const override;
  void SetTextEditCommandForNextKeyEvent(ui::TextEditCommand command) override;
  ukm::SourceId GetClientSourceForMetrics() const override;
  bool ShouldDoLearning() override;

  // ui::internal::InputMethodDelegate:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* event,
      base::OnceCallback<void(bool)> ack_callback) override;

  ws::mojom::TextInputClientPtr remote_client_;
  ui::TextInputType text_input_type_;
  ui::TextInputMode text_input_mode_;
  base::i18n::TextDirection text_direction_;
  int text_input_flags_;
  gfx::Rect caret_bounds_;

  // Callbacks supplied to DispatchKeyEventPostIME() are added here. When the
  // response from the remote side is received
  // (OnDispatchKeyEventPostIMECompleted()), the callback is removed and run.
  // This is done to ensure if we are destroyed all the callbacks are run.
  // This is necessary as the callbacks may have originated from a remote
  // client.
  base::queue<base::OnceCallback<void(bool)>> pending_callbacks_;

  base::WeakPtrFactory<RemoteTextInputClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RemoteTextInputClient);
};

#endif  // CHROME_BROWSER_UI_VIEWS_IME_DRIVER_REMOTE_TEXT_INPUT_CLIENT_H_
