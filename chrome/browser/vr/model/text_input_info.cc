// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/model/text_input_info.h"

#include <algorithm>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

namespace vr {

namespace {

size_t CommonPrefixLength(const std::u16string a, const std::u16string b) {
  size_t a_len = a.length();
  size_t b_len = b.length();
  size_t i = 0;
  while (i < a_len && i < b_len && a[i] == b[i]) {
    i++;
  }
  return i;
}

}  // namespace

TextInputInfo::TextInputInfo()
    : TextInputInfo(u"",
                    0,
                    0,
                    kDefaultCompositionIndex,
                    kDefaultCompositionIndex) {}

TextInputInfo::TextInputInfo(std::u16string t)
    : TextInputInfo(t,
                    t.length(),
                    t.length(),
                    kDefaultCompositionIndex,
                    kDefaultCompositionIndex) {}

TextInputInfo::TextInputInfo(std::u16string t, int sel_start, int sel_end)
    : TextInputInfo(t,
                    sel_start,
                    sel_end,
                    kDefaultCompositionIndex,
                    kDefaultCompositionIndex) {}

TextInputInfo::TextInputInfo(std::u16string t,
                             int sel_start,
                             int sel_end,
                             int comp_start,
                             int comp_end)
    : text(t),
      selection_start(sel_start),
      selection_end(sel_end),
      composition_start(comp_start),
      composition_end(comp_end) {
  ClampIndices();
}

TextInputInfo::TextInputInfo(const TextInputInfo& other) = default;

TextInputInfo& TextInputInfo::operator=(const TextInputInfo& other) = default;

bool TextInputInfo::operator==(const TextInputInfo& other) const {
  return text == other.text && selection_start == other.selection_start &&
         selection_end == other.selection_end &&
         composition_start == other.composition_start &&
         composition_end == other.composition_end;
}

bool TextInputInfo::operator!=(const TextInputInfo& other) const {
  return !(*this == other);
}

size_t TextInputInfo::SelectionSize() const {
  return std::abs(selection_end - selection_start);
}

size_t TextInputInfo::CompositionSize() const {
  return composition_end - composition_start;
}

std::u16string TextInputInfo::CommittedTextBeforeCursor() const {
  if (composition_start == composition_end)
    return text.substr(0, selection_start);
  return text.substr(0, composition_start);
}

std::u16string TextInputInfo::ComposingText() const {
  if (composition_start == composition_end)
    return u"";
  return text.substr(composition_start, CompositionSize());
}

std::string TextInputInfo::ToString() const {
  return base::StringPrintf("t(%s) s(%d, %d) c(%d, %d)",
                            base::UTF16ToUTF8(text).c_str(), selection_start,
                            selection_end, composition_start, composition_end);
}

void TextInputInfo::ClampIndices() {
  const int len = text.length();
  selection_start = std::min(selection_start, len);
  selection_end = std::min(selection_end, len);
  if (selection_end < selection_start)
    selection_end = selection_start;
  composition_start = std::min(composition_start, len);
  composition_end = std::min(composition_end, len);
  if (composition_end <= composition_start) {
    composition_start = kDefaultCompositionIndex;
    composition_end = kDefaultCompositionIndex;
  }
}

EditedText::EditedText() = default;

EditedText::EditedText(const EditedText& other) = default;

EditedText& EditedText::operator=(const EditedText& other) = default;

EditedText::EditedText(const TextInputInfo& new_current)
    : current(new_current) {}

EditedText::EditedText(const TextInputInfo& new_current,
                       const TextInputInfo& new_previous)
    : current(new_current), previous(new_previous) {}

EditedText::EditedText(std::u16string t) : current(t) {}

bool EditedText::operator==(const EditedText& other) const {
  return current == other.current && previous == other.previous;
}

void EditedText::Update(const TextInputInfo& info) {
  previous = current;
  current = info;
}

std::string EditedText::ToString() const {
  return current.ToString() + ", previously " + previous.ToString();
}

TextEdits EditedText::GetDiff() const {
  TextEdits edits;
  if (current == previous)
    return edits;

  int common_prefix_length =
      CommonPrefixLength(current.CommittedTextBeforeCursor(),
                         previous.CommittedTextBeforeCursor());
  bool had_composition =
      previous.CompositionSize() > 0 && current.CompositionSize() == 0;
  // If the composition changes while there was a composition previously, we
  // first finish the previous composition by clearing then commiting it, then
  // we set the new composition.
  bool new_composition =
      previous.composition_start != current.composition_start &&
      previous.CompositionSize() > 0;
  if (had_composition || new_composition) {
    edits.push_back(TextEditAction(TextEditActionType::CLEAR_COMPOSING_TEXT));
  }

  int to_delete = 0;
  // We only want to delete text if the was no selection previously. In the case
  // where there was a selection, its the editor's responsibility to ensure that
  // the selected text gets modified when a new edit occurs.
  bool had_selection =
      previous.SelectionSize() > 0 && current.SelectionSize() == 0;
  if (!had_selection) {
    to_delete =
        previous.CommittedTextBeforeCursor().size() - common_prefix_length;
    if (to_delete > 0) {
      DCHECK(!had_composition);
      edits.push_back(
          TextEditAction(TextEditActionType::DELETE_TEXT, u"", -to_delete));
    }
  }

  int to_commit =
      current.CommittedTextBeforeCursor().size() - common_prefix_length;
  if (to_commit > 0 || had_selection) {
    DCHECK_EQ(0, to_delete);
    edits.push_back(TextEditAction(TextEditActionType::COMMIT_TEXT,
                                   current.CommittedTextBeforeCursor().substr(
                                       common_prefix_length, to_commit),
                                   to_commit));
  }
  if (current.CompositionSize() > 0) {
    int cursor = current.CompositionSize();
    if (!new_composition) {
      cursor = current.CompositionSize() - previous.CompositionSize();
    }
    edits.push_back(TextEditAction(TextEditActionType::SET_COMPOSING_TEXT,
                                   current.ComposingText(), cursor));
  }

  return edits;
}

static_assert(sizeof(std::u16string) + 16 == sizeof(TextInputInfo),
              "If new fields are added to TextInputInfo, we must explicitly "
              "bump this size and update operator==");

}  // namespace vr
