// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_TEXT_DUMP_RESULT_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_TEXT_DUMP_RESULT_H_

#include <optional>
#include <set>
#include <string>

#include "components/optimization_guide/content/mojom/page_text_service.mojom.h"
#include "content/public/browser/global_routing_id.h"

namespace optimization_guide {

// This class contains the text dump from a single renderer and associated
// metadata. The lifecycle of this class is in two phases, preliminary and
// final. Preliminary is when the frame metadata is known, but the text dump has
// not been received yet. Once the text dump is received, then the dump is
// completed.
class FrameTextDumpResult {
 public:
  ~FrameTextDumpResult();
  FrameTextDumpResult(const FrameTextDumpResult&);

  // Creates a preliminary instance with the given metadata.
  static FrameTextDumpResult Initialize(mojom::TextDumpEvent event,
                                        content::GlobalRenderFrameHostId rfh_id,
                                        bool amp_frame,
                                        int unique_navigation_id);

  // Returns a copy of |this| that is completed with |contents|. This is only
  // expected to be called once on Preliminary instances.
  FrameTextDumpResult CompleteWithContents(
      const std::u16string& contents) const;

  // Whether the class instance is completed yet.
  bool IsCompleted() const;

  // The text dump contents. Set only for completed instances. Note that
  // the string must be treated as untrusted data.
  const std::optional<std::u16string>& contents() const { return contents_; }

  // The text dump contents, decoded to UTF-8 as a best effort. Set only for
  // completed instances. Note that the string must be treated as untrusted
  // data.
  std::optional<std::string> utf8_contents() const;

  // The event at which the text dump is taken. Set for both preliminary and
  // completed instances.
  mojom::TextDumpEvent event() const { return event_; }

  // The unique identifier for the content::RenderFrameHost that the text dump
  // was taken in. Set for both preliminary and completed instances.
  content::GlobalRenderFrameHostId rfh_id() const { return rfh_id_; }

  // The unique id of the visible navigation for this frame dump, taken from the
  // visible NavigationEntry.
  int unique_navigation_id() const { return unique_navigation_id_; }

  // Whether the frame the text dump is taken in an AMP frame. Set for both
  // preliminary and completed instances.
  bool amp_frame() const { return amp_frame_; }

  // These objects are sorted in the following manner:
  // * AMP frames first - When there are AMP frames on a page, it is expected
  // that they will contain the most content.
  // * Longer contents first - Most consumers will only be interested in some of
  // the page text. In this case, ensure that the biggest blob of text comes
  // first since that is most likely to be the main content on the page.
  // * Later events first - The later in the page's lifetime a text dump is
  // taken, the more likely that it is complete.
  // * Everything else is just done willy-nilly for completeness of equality
  // checking.
  inline bool operator<(const FrameTextDumpResult& rhs) const {
    if (amp_frame() != rhs.amp_frame()) {
      return amp_frame();
    }

    size_t lhs_size = contents() ? contents()->size() : 0;
    size_t rhs_size = rhs.contents() ? rhs.contents()->size() : 0;
    if (lhs_size != rhs_size) {
      // Note the reverse ordering to put longer contents first.
      return lhs_size > rhs_size;
    }

    if (event() != rhs.event()) {
      // Note the reverse ordering to put later events first.
      return event() > rhs.event();
    }

    return std::tie(rfh_id_, contents_, unique_navigation_id_) <
           std::tie(rhs.rfh_id_, rhs.contents_, rhs.unique_navigation_id_);
  }

  inline bool operator==(const FrameTextDumpResult& other) const {
    return std::tie(event_, contents_, rfh_id_, amp_frame_,
                    unique_navigation_id_) ==
           std::tie(other.event_, other.contents_, other.rfh_id_,
                    other.amp_frame_, other.unique_navigation_id_);
  }

 private:
  FrameTextDumpResult();

  mojom::TextDumpEvent event_;
  std::optional<std::u16string> contents_;
  content::GlobalRenderFrameHostId rfh_id_;
  bool amp_frame_ = false;
  int unique_navigation_id_ = -1;
};

// Contains 0 or more FrameTextDumpResults from the same page load.
class PageTextDumpResult {
 public:
  PageTextDumpResult();
  PageTextDumpResult(const PageTextDumpResult&);
  ~PageTextDumpResult();

  // Adds another frame text dump to |this|.
  void AddFrameTextDumpResult(const FrameTextDumpResult& frame_result);

  // Returns the concatenation of all AMP frames. nullopt if no AMP frames are
  // present. Note that the string must be treated as untrusted data.
  std::optional<std::string> GetAMPTextContent() const;

  // Returns the concatenation of the mainframe. nullopt if not present.
  // Note that the string must be treated as untrusted data.
  std::optional<std::string> GetMainFrameTextContent() const;

  // Returns the concatenation of all frames, AMP or main. nullopt if |empty()|.
  // Note that the string must be treated as untrusted data.
  std::optional<std::string> GetAllFramesTextContent() const;

  bool empty() const { return frame_results_.empty(); }

  const std::set<FrameTextDumpResult>& frame_results() const {
    return frame_results_;
  }

  inline bool operator==(const PageTextDumpResult& other) const {
    return frame_results_ == other.frame_results_;
  }

 private:
  std::set<FrameTextDumpResult> frame_results_;
};

// Useful for debugging.
std::ostream& operator<<(std::ostream& os, const FrameTextDumpResult& frame);
std::ostream& operator<<(std::ostream& os, const PageTextDumpResult& page);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_TEXT_DUMP_RESULT_H_
