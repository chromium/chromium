// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/omnibox_text_field.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/test/mock_text_input_delegate.h"
#include "chrome/browser/vr/test/mock_ui_browser_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::StrictMock;

namespace vr {

void PrintTo(const AutocompleteRequest& request, std::ostream* os) {
  *os << "Input '" << request.text << "', cursor at " << request.cursor_position
      << ", " << (request.prevent_inline_autocomplete ? "prevent" : "allow")
      << " in-line match";
}

class OmniboxTest : public testing::Test {
 public:
  void SetUp() override {
    omnibox_ = std::make_unique<OmniboxTextField>(
        10, base::RepeatingCallback<void(const EditedText&)>(),
        base::BindRepeating(&UiBrowserInterface::StartAutocomplete,
                            base::Unretained(&browser_)),
        base::BindRepeating(&UiBrowserInterface::StopAutocomplete,
                            base::Unretained(&browser_)));
    omnibox_->OnFocusChanged(true);
    omnibox_->SetTextInputDelegate(&text_input_delegate_);
    omnibox_->set_allow_inline_autocomplete(true);
  }

 protected:
  void SetInput(const std::string& text,
                int selection_start,
                int selection_end,
                const std::string& previous_text,
                int previous_selection_start,
                int previous_selection_end) {
    EditedText info;
    info.current.text = base::UTF8ToUTF16(text);
    info.current.selection_start = selection_start;
    info.current.selection_end = selection_end;
    info.previous.text = base::UTF8ToUTF16(previous_text);
    info.previous.selection_start = previous_selection_start;
    info.previous.selection_end = previous_selection_end;
    omnibox_->UpdateInput(info);
  }

  void SetAutocompletion(const std::string& text,
                         const std::string& completion) {
    omnibox_->SetAutocompletion(
        Autocompletion(base::UTF8ToUTF16(text), base::UTF8ToUTF16(completion)));
  }

  static constexpr bool kPreventInline = true;
  static constexpr bool kAllowInline = false;

  void ExpectAutocompleteRequest(const std::string& input,
                                 int cursor_position,
                                 bool prevent_inline_autocomplete) {
    AutocompleteRequest request;
    request.text = base::UTF8ToUTF16(input);
    request.cursor_position = cursor_position;
    request.prevent_inline_autocomplete = prevent_inline_autocomplete;
    EXPECT_CALL(browser_, StartAutocomplete(request));
  }

  void ExpectKeyboardUpdate(const std::string& text,
                            int selection_start,
                            int selection_end) {
    TextInputInfo info;
    info.text = base::UTF8ToUTF16(text);
    info.selection_start = selection_start;
    info.selection_end = selection_end;
    EXPECT_CALL(text_input_delegate_, UpdateInput(info));
  }

  void ExpectKeyboardUpdate(const std::string& text, int cursor_position) {
    ExpectKeyboardUpdate(text, cursor_position, cursor_position);
  }

  void VerifyMocks() {
    testing::Mock::VerifyAndClear(&browser_);
    testing::Mock::VerifyAndClear(&text_input_delegate_);
  }

  std::unique_ptr<OmniboxTextField> omnibox_;
  testing::Sequence in_sequence_;
  StrictMock<MockUiBrowserInterface> browser_;
  StrictMock<MockTextInputDelegate> text_input_delegate_;
};

TEST_F(OmniboxTest, HandleInput) {
  {
    // Type "w".
    SCOPED_TRACE(__LINE__);
    ExpectAutocompleteRequest("w", 1, kAllowInline);
    ExpectKeyboardUpdate("w", 1);
    SetInput("w", 1, 1, "", 0, 0);
    VerifyMocks();
  }
  {
    // Type "i".
    SCOPED_TRACE(__LINE__);
    ExpectAutocompleteRequest("wi", 2, kAllowInline);
    ExpectKeyboardUpdate("wi", 2);
    SetInput("wi", 2, 2, "w", 1, 1);
    VerifyMocks();
  }
  {
    // Move the cursor to the beginning, and verify that a request with
    // prevent inline match is sent, with the correct cursor position.
    SCOPED_TRACE(__LINE__);
    ExpectAutocompleteRequest("wi", 0, kPreventInline);
    ExpectKeyboardUpdate("wi", 0);
    SetInput("wi", 0, 0, "wi", 2, 2);
    VerifyMocks();
  }
  {
    // Type at the beginning, ensuring that in-line match is disabled.
    SCOPED_TRACE(__LINE__);
    ExpectKeyboardUpdate(".wi", 1);
    ExpectAutocompleteRequest(".wi", 1, kPreventInline);
    SetInput(".wi", 1, 1, "wi", 0, 0);
    VerifyMocks();
  }
  {
    // Backspace to remove the last character.
    SCOPED_TRACE(__LINE__);
    ExpectAutocompleteRequest("wi", 0, kPreventInline);
    ExpectKeyboardUpdate("wi", 0);
    SetInput("wi", 0, 0, ".wi", 1, 1);
    VerifyMocks();
  }
  {
    // Return the cursor to the end.
    SCOPED_TRACE(__LINE__);
    ExpectAutocompleteRequest("wi", 2, kPreventInline);
    ExpectKeyboardUpdate("wi", 2);
    SetInput("wi", 2, 2, "wi", 0, 0);
    VerifyMocks();
  }
  {
    // Type "k".  The autocomplete request should return to allowing inline.
    SCOPED_TRACE(__LINE__);
    ExpectAutocompleteRequest("wik", 3, kAllowInline);
    ExpectKeyboardUpdate("wik", 3);
    SetInput("wik", 3, 3, "wi", 2, 2);
    VerifyMocks();
  }
}

TEST_F(OmniboxTest, HandleMatches) {
  {
    // Type "w".
    SCOPED_TRACE(__LINE__);
    ExpectAutocompleteRequest("w", 1, kAllowInline);
    ExpectKeyboardUpdate("w", 1);
    SetInput("w", 1, 1, "", 0, 0);
    VerifyMocks();
  }
  {
    // Supply a match, and ensure no new autocomplete request is sent.
    SCOPED_TRACE(__LINE__);
    ExpectKeyboardUpdate("wiki", 4, 1);
    SetAutocompletion("w", "iki");
    VerifyMocks();
  }
  {
    // Supply a new match that replaces the previous.
    SCOPED_TRACE(__LINE__);
    ExpectKeyboardUpdate("wikipedia", 9, 1);
    SetAutocompletion("w", "ikipedia");
    VerifyMocks();
  }
  {
    // Supply a stale match with a different input text, and ensure it's
    // ignored.
    SCOPED_TRACE(__LINE__);
    SetAutocompletion("wi", "kipedia");
    VerifyMocks();
  }
  {
    // Clear the match with a backspace.  Autocomplete request should prevent
    // inline.
    SCOPED_TRACE(__LINE__);
    ExpectAutocompleteRequest("w", 1, kPreventInline);
    ExpectKeyboardUpdate("w", 1);
    SetInput("w", 1, 1, "wikipedia", 1, 9);
    VerifyMocks();
  }
  {
    // Restore a match.
    SCOPED_TRACE(__LINE__);
    ExpectKeyboardUpdate("wikipedia", 9, 1);
    SetAutocompletion("w", "ikipedia");
    VerifyMocks();
  }
  {
    // Move the cursor into the match, ensuring that the match is incorporated
    // into the next autocomplete request, but that in-line is disabled.
    SCOPED_TRACE(__LINE__);
    ExpectKeyboardUpdate("wikipedia", 4);
    ExpectAutocompleteRequest("wikipedia", 4, kPreventInline);
    SetInput("wikipedia", 4, 4, "wikipedia", 1, 9);
    VerifyMocks();
  }
  {
    // Type into the middle of what used to be the match, ensuring that in-line
    // stays disabled.
    SCOPED_TRACE(__LINE__);
    ExpectKeyboardUpdate("wikippedia", 5);
    ExpectAutocompleteRequest("wikippedia", 5, kPreventInline);
    SetInput("wikippedia", 5, 5, "wikipedia", 4, 4);
    VerifyMocks();
  }
}

TEST_F(OmniboxTest, StopAutocompleteWhenDisabled) {
  ExpectAutocompleteRequest("w", 1, kAllowInline);
  ExpectKeyboardUpdate("w", 1);
  omnibox_->SetEnabled(true);
  SetInput("w", 1, 1, "", 0, 0);
  VerifyMocks();

  EXPECT_CALL(browser_, StopAutocomplete());
  omnibox_->SetEnabled(false);
}

}  // namespace vr
