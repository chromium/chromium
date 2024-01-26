// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_text_dump_result.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

namespace optimization_guide {

namespace {

std::string TextDumpEventToString(mojom::TextDumpEvent value) {
  switch (value) {
    case mojom::TextDumpEvent::kFirstLayout:
      return "kFirstLayout";
    case mojom::TextDumpEvent::kFinishedLoad:
      return "kFinishedLoad";
  }
}

}  // namespace

PageTextDumpResult::PageTextDumpResult() = default;
PageTextDumpResult::PageTextDumpResult(const PageTextDumpResult&) = default;
PageTextDumpResult::~PageTextDumpResult() = default;

void PageTextDumpResult::AddFrameTextDumpResult(
    const FrameTextDumpResult& frame_result) {
  DCHECK(frame_result.IsCompleted());
  frame_results_.emplace(frame_result);
}

std::optional<std::string> PageTextDumpResult::GetAMPTextContent() const {
  if (empty()) {
    return std::nullopt;
  }

  // AMP frames are sorted in beginning, so if there are none then return null.
  if (!frame_results_.begin()->amp_frame()) {
    return std::nullopt;
  }

  std::vector<std::string> amp_text;
  for (const FrameTextDumpResult& frame_result : frame_results_) {
    DCHECK(frame_result.utf8_contents());

    if (!frame_result.amp_frame()) {
      break;
    }

    amp_text.push_back(*frame_result.utf8_contents());
  }
  DCHECK(!amp_text.empty());

  return base::JoinString(amp_text, " ");
}

std::optional<std::string> PageTextDumpResult::GetMainFrameTextContent() const {
  if (empty()) {
    return std::nullopt;
  }

  // Mainframes are sorted to the end.
  if (frame_results_.rbegin()->amp_frame()) {
    return std::nullopt;
  }

  // There should only be one mainframe.
  DCHECK(frame_results_.rbegin()->utf8_contents());
  return *frame_results_.rbegin()->utf8_contents();
}

std::optional<std::string> PageTextDumpResult::GetAllFramesTextContent() const {
  if (empty()) {
    return std::nullopt;
  }

  std::vector<std::string> text;
  for (const FrameTextDumpResult& frame_result : frame_results_) {
    DCHECK(frame_result.utf8_contents());
    text.push_back(*frame_result.utf8_contents());
  }
  DCHECK(!text.empty());

  return base::JoinString(text, " ");
}

FrameTextDumpResult::FrameTextDumpResult() = default;
FrameTextDumpResult::~FrameTextDumpResult() = default;
FrameTextDumpResult::FrameTextDumpResult(const FrameTextDumpResult&) = default;

// static
FrameTextDumpResult FrameTextDumpResult::Initialize(
    mojom::TextDumpEvent event,
    content::GlobalRenderFrameHostId rfh_id,
    bool amp_frame,
    int unique_navigation_id) {
  FrameTextDumpResult result;
  result.event_ = event;
  result.rfh_id_ = rfh_id;
  result.amp_frame_ = amp_frame;
  result.unique_navigation_id_ = unique_navigation_id;
  return result;
}

FrameTextDumpResult FrameTextDumpResult::CompleteWithContents(
    const std::u16string& contents) const {
  DCHECK(!IsCompleted());

  FrameTextDumpResult copy = *this;
  // Always trim whitespace from |contents| because it can non-deterministically
  // have trailing whitespace which makes testing and parsing harder.
  copy.contents_ = contents;
  base::TrimWhitespace(contents, base::TRIM_ALL, &(copy.contents_.value()));
  return copy;
}

bool FrameTextDumpResult::IsCompleted() const {
  return !!contents();
}

std::optional<std::string> FrameTextDumpResult::utf8_contents() const {
  if (!contents_) {
    return std::nullopt;
  }
  return base::UTF16ToUTF8(*contents_);
}

std::ostream& operator<<(std::ostream& os, const FrameTextDumpResult& frame) {
  return os << base::StringPrintf(
             "event:%s rfh_id:(%d,%d) amp_frame:%s unique_navigation_id:%d "
             "contents:%s",
             TextDumpEventToString(frame.event()).c_str(),
             frame.rfh_id().child_id, frame.rfh_id().frame_routing_id,
             frame.amp_frame() ? "true" : "false", frame.unique_navigation_id(),
             frame.utf8_contents().value_or("null").c_str());
}

std::ostream& operator<<(std::ostream& os, const PageTextDumpResult& page) {
  for (const FrameTextDumpResult& frame : page.frame_results()) {
    os << frame << "\n";
  }
  return os;
}

}  // namespace optimization_guide
