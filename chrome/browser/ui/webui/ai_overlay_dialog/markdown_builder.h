// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_MARKDOWN_BUILDER_H_
#define CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_MARKDOWN_BUILDER_H_

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/memory/raw_ref.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "url/gurl.h"

namespace ttc {

// Converts optimization_guide::proto::AnnotatedPageContent into a markdown
// representation. This class is roughly derived from Glic's APC to Markdown
// generator.
class MarkdownBuilder {
 public:
  static std::unordered_map<std::string, int> GenerateUrlHashes(
      const optimization_guide::proto::AnnotatedPageContent& page_content);

  MarkdownBuilder(
      const optimization_guide::proto::AnnotatedPageContent& page_content,
      const GURL& page_url);
  ~MarkdownBuilder();

  MarkdownBuilder(const MarkdownBuilder&) = delete;
  MarkdownBuilder& operator=(const MarkdownBuilder&) = delete;

  // Builds and returns the markdown representation of the page content.
  std::string Build();

 private:
  struct WalkState {
    WalkState();
    ~WalkState();
    std::vector<std::string> lines;
    int32_t ordered_list_item_index = 1;
  };

  void WalkContentNodes(
      std::vector<const optimization_guide::proto::ContentNode*>& parent_chain,
      const optimization_guide::proto::ContentNode& node,
      const optimization_guide::proto::DocumentIdentifier& document_identifier);

  void ProcessNodeBeforeSubtree(
      const std::vector<const optimization_guide::proto::ContentNode*>&
          parent_chain,
      const optimization_guide::proto::ContentNode& node,
      const optimization_guide::proto::DocumentIdentifier& document_identifier);

  void ProcessNodeAfterSubtree(
      const optimization_guide::proto::ContentNode& node,
      const optimization_guide::proto::DocumentIdentifier& document_identifier);

  void AddMarkdownBeforeSubtree(
      const std::vector<const optimization_guide::proto::ContentNode*>&
          parent_chain,
      const optimization_guide::proto::ContentNode& node,
      const optimization_guide::proto::DocumentIdentifier& document_identifier);

  void AddMarkdownAfterSubtree(
      const optimization_guide::proto::ContentNode& node);

  std::string GetContent(
      const std::vector<const optimization_guide::proto::ContentNode*>&
          parent_chain,
      const optimization_guide::proto::ContentNode& node);

  std::string FormatText(
      const optimization_guide::proto::TextInfo& text_info,
      const std::vector<const optimization_guide::proto::ContentNode*>&
          parent_chain);

  std::string FormatImage(
      const optimization_guide::proto::ImageInfo& image_info);

  std::string FormatVideo(
      const optimization_guide::proto::VideoData& video_data);

  std::string GetListItemPrefix(
      const std::vector<const optimization_guide::proto::ContentNode*>&
          parent_chain,
      const optimization_guide::proto::ContentNode& node);

  void AddSpace();
  void SetUpLine(
      const std::vector<const optimization_guide::proto::ContentNode*>&
          parent_chain,
      const optimization_guide::proto::ContentNode& node);
  void CompleteLine(const optimization_guide::proto::ContentNode& node);

  bool IsHeading(
      const optimization_guide::proto::ContentAttributes& attributes);
  bool IsTableCell(
      const optimization_guide::proto::ContentAttributes& attributes);
  bool IsAnchor(const optimization_guide::proto::ContentAttributes& attributes);
  bool IsFormControlInParentChain(
      const std::vector<const optimization_guide::proto::ContentNode*>&
          parent_chain);

  std::string JoinLines();

  const raw_ref<const optimization_guide::proto::AnnotatedPageContent>
      page_content_;
  WalkState walk_state_;
  GURL page_url_;
  std::unordered_map<std::string, int> url_to_hash_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_MARKDOWN_BUILDER_H_
