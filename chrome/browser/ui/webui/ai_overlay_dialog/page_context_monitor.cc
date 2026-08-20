// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/page_context_monitor.h"

#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/page_content_annotations/page_content_screenshot_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/re2/src/re2/re2.h"

namespace ttc {

const base::TimeDelta kEmptyPageRetryDelay = base::Seconds(2);

using page_content_annotations::PageContentScreenshotServiceFactory;

namespace {

std::string FindFirstTextInSubtree(
    const optimization_guide::proto::ContentNode& node) {
  if (node.has_content_attributes()) {
    const auto& attrs = node.content_attributes();
    if (attrs.has_text_data() && !attrs.text_data().text_content().empty()) {
      return attrs.text_data().text_content();
    }
    if (!attrs.label().empty()) {
      return attrs.label();
    }
  }
  for (const auto& child : node.children_nodes()) {
    std::string text = FindFirstTextInSubtree(child);
    if (!text.empty()) {
      return text;
    }
  }
  return "";
}

void ConvertContentNodesToMojo(
    const google::protobuf::RepeatedPtrField<
        optimization_guide::proto::ContentNode>& proto_nodes,
    std::vector<ai_overlay_dialog::mojom::PageContentNodePtr>& mojo_nodes) {
  for (int i = 0; i < proto_nodes.size(); ++i) {
    const auto& proto_node = proto_nodes[i];
    auto mojo_node = ai_overlay_dialog::mojom::PageContentNode::New();

    if (proto_node.has_content_attributes()) {
      const auto& attrs = proto_node.content_attributes();
      mojo_node->dom_node_id = attrs.common_ancestor_dom_node_id();

      if (attrs.attribute_type() ==
          optimization_guide::proto::ContentAttributeType::
              CONTENT_ATTRIBUTE_IMAGE) {
        mojo_node->tag_name = "img";
        if (attrs.has_image_data()) {
          mojo_node->text = attrs.image_data().image_caption();
        }
      }

      if (attrs.has_text_data()) {
        mojo_node->text = attrs.text_data().text_content();
      }
      if (attrs.has_anchor_data()) {
        mojo_node->tag_name = "a";
        mojo_node->url = GURL(attrs.anchor_data().url());
        mojo_node->is_interactive = true;
        mojo_node->role = ai_overlay_dialog::mojom::NodeRole::kLink;
      }
      if (attrs.has_form_control_data()) {
        const auto& fc = attrs.form_control_data();
        mojo_node->is_interactive = true;
        mojo_node->is_checked = fc.is_checked();
        mojo_node->value = fc.field_value();
        mojo_node->placeholder = fc.placeholder();

        std::string label = attrs.has_text_data()
                                ? attrs.text_data().text_content()
                                : attrs.label();
        if (label.empty()) {
          label = FindFirstTextInSubtree(proto_node);
        }

        // Lookahead to next sibling if label is still empty
        bool consumed_next_sibling = false;
        if (label.empty() && i + 1 < proto_nodes.size()) {
          const auto& next_node = proto_nodes[i + 1];
          if (next_node.has_content_attributes()) {
            const auto& next_attrs = next_node.content_attributes();
            if (!next_attrs.has_form_control_data() &&
                !next_attrs.has_anchor_data()) {
              std::string next_text = FindFirstTextInSubtree(next_node);
              if (!next_text.empty()) {
                label = next_text;
                consumed_next_sibling = true;
              }
            }
          }
        }
        mojo_node->text = label;

        switch (fc.form_control_type()) {
          case optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_CHECKBOX:
            mojo_node->role = ai_overlay_dialog::mojom::NodeRole::kCheckbox;
            break;
          case optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_RADIO:
            mojo_node->role = ai_overlay_dialog::mojom::NodeRole::kRadio;
            break;
          case optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_TEXT:
          case optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_EMAIL:
          case optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_NUMBER:
          case optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_PASSWORD:
          case optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_SEARCH:
          case optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_TELEPHONE:
          case optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_URL:
          case optimization_guide::proto::FORM_CONTROL_TYPE_TEXT_AREA:
          case optimization_guide::proto::FORM_CONTROL_TYPE_CONTENT_EDITABLE:
            mojo_node->role = ai_overlay_dialog::mojom::NodeRole::kTextbox;
            break;
          case optimization_guide::proto::FORM_CONTROL_TYPE_BUTTON_BUTTON:
          case optimization_guide::proto::FORM_CONTROL_TYPE_BUTTON_SUBMIT:
          case optimization_guide::proto::FORM_CONTROL_TYPE_BUTTON_RESET:
          case optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_BUTTON:
            mojo_node->role = ai_overlay_dialog::mojom::NodeRole::kButton;
            break;
          case optimization_guide::proto::FORM_CONTROL_TYPE_SELECT_ONE:
          case optimization_guide::proto::FORM_CONTROL_TYPE_SELECT_MULTIPLE:
            mojo_node->role = ai_overlay_dialog::mojom::NodeRole::kCombobox;
            break;
          default:
            mojo_node->role = ai_overlay_dialog::mojom::NodeRole::kInput;
            break;
        }

        if (consumed_next_sibling) {
          ++i;
        }
      } else if (attrs.attribute_type() ==
                 optimization_guide::proto::CONTENT_ATTRIBUTE_ANCHOR) {
        mojo_node->is_interactive = true;
        mojo_node->role = ai_overlay_dialog::mojom::NodeRole::kLink;
      }

      if (attrs.has_interaction_info()) {
        const auto& ii = attrs.interaction_info();
        if (ii.clickability_reasons_size() > 0 || ii.is_clickable()) {
          mojo_node->is_interactive = true;
        }
      }
    }

    if (proto_node.children_nodes_size() > 0) {
      ConvertContentNodesToMojo(proto_node.children_nodes(),
                                mojo_node->children);
    }
    mojo_nodes.push_back(std::move(mojo_node));
  }
}

std::string FindUrlForDomNodeId(
    const optimization_guide::proto::ContentNode& node,
    int target_id) {
  if (node.has_content_attributes()) {
    const auto& attrs = node.content_attributes();
    if (attrs.has_anchor_data()) {
      int url_hash = static_cast<int>(
          base::PersistentHash(attrs.anchor_data().url()) % 10000);
      if (attrs.common_ancestor_dom_node_id() == target_id ||
          url_hash == target_id) {
        return attrs.anchor_data().url();
      }
    }
  }
  for (const auto& child : node.children_nodes()) {
    std::string url = FindUrlForDomNodeId(child, target_id);
    if (!url.empty()) {
      return url;
    }
  }
  return "";
}

}  // namespace

PageContextMonitor::PageContextMonitor(BrowserWindowInterface& window,
                                       AiOverlayDialogPageHandler& page_handler)
    : window_(window), page_handler_(page_handler) {
  active_tab_subscription_ =
      window.RegisterActiveTabDidChange(base::BindRepeating(
          &PageContextMonitor::OnActiveTabChanged, base::Unretained(this)));
  OnActiveTabChanged(&window);
}

PageContextMonitor::~PageContextMonitor() = default;

void PageContextMonitor::PrimaryPageChanged(content::Page& page) {
  last_page_content_.reset();
  page_handler_->DidChangePage(web_contents()->GetLastCommittedURL(),
                               web_contents()->GetTitle(), std::nullopt);
  did_retry_first_fetch_ = false;
  StartNewFetch();
}

void PageContextMonitor::DidStopLoading() {
  if (fetch_waiting_on_load_) {
    StartNewFetch();
  }
}

void PageContextMonitor::OnActiveTabChanged(BrowserWindowInterface* window) {
  CHECK_EQ(window, &window_.get());

  tabs::TabInterface* active_tab = window_->GetActiveTabInterface();
  Observe(active_tab ? active_tab->GetContents() : nullptr);
  last_page_content_.reset();

  if (!active_tab) {
    return;
  }

  page_handler_->DidChangePage(web_contents()->GetLastCommittedURL(),
                               web_contents()->GetTitle(), std::nullopt);
  StartNewFetch();
}

void PageContextMonitor::StartNewFetch() {
  fetch_waiting_on_load_ = false;
  fetcher_.reset();

  content::WebContents* contents = web_contents();
  if (!contents) {
    return;
  }

  if (contents->IsLoading()) {
    fetch_waiting_on_load_ = true;
    return;
  }

  fetcher_ = std::make_unique<page_content_annotations::PageContextFetcher>(
      base::BindRepeating([](content::BrowserContext* context) {
        return PageContentScreenshotServiceFactory::GetForProfile(
            Profile::FromBrowserContext(context));
      }),
      /*progress_listener=*/nullptr);

  page_content_annotations::FetchPageContextOptions options;
  options.annotated_page_content_options =
      optimization_guide::DefaultAIPageContentOptions(
          /* on_critical_path =*/true);
  options.annotated_page_content_options->max_meta_elements = 32;

  fetcher_->FetchStart(*contents, options,
                       base::BindOnce(&PageContextMonitor::OnFetchComplete,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void PageContextMonitor::OnFetchComplete(
    page_content_annotations::FetchPageContextResultCallbackArg result) {
  if (!result.has_value()) {
    LOG(WARNING) << "FetchPageContextResult returned error";
    return;
  }

  const page_content_annotations::FetchPageContextResult& fetch_result =
      **result;

  if (fetch_result.annotated_page_content_result.has_value()) {
    last_page_content_ =
        fetch_result.annotated_page_content_result.value().proto;
    ai_overlay_dialog::mojom::PageContentNodePtr root_mojo_node;
    if (last_page_content_->has_root_node()) {
      const auto& root_proto = last_page_content_->root_node();
      root_mojo_node = ai_overlay_dialog::mojom::PageContentNode::New();
      if (root_proto.has_content_attributes()) {
        const auto& attrs = root_proto.content_attributes();
        root_mojo_node->dom_node_id = attrs.common_ancestor_dom_node_id();
        if (attrs.has_text_data()) {
          root_mojo_node->text = attrs.text_data().text_content();
        }
      }
      ConvertContentNodesToMojo(root_proto.children_nodes(),
                                root_mojo_node->children);
    }

    // If the page looks mostly empty, crudely wait a bit and retry in case the
    // load comes before content is shown.
    if (!did_retry_first_fetch_ && !root_mojo_node) {
      did_retry_first_fetch_ = true;
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&PageContextMonitor::StartNewFetch,
                         weak_ptr_factory_.GetWeakPtr()),
          kEmptyPageRetryDelay);
    }

    page_handler_->UpdateCurrentPageContext(web_contents()->GetTitle(),
                                            std::move(root_mojo_node));
  }
}

std::string PageContextMonitor::GetUrlForHash(
    const std::string& hash_str) const {
  if (!last_page_content_.has_value()) {
    return "";
  }
  std::string_view hash_sv(hash_str);
  while (!hash_sv.empty() &&
         (hash_sv.front() == '{' || hash_sv.front() == '#')) {
    hash_sv.remove_prefix(1);
  }
  while (!hash_sv.empty() && hash_sv.back() == '}') {
    hash_sv.remove_suffix(1);
  }
  int target_hash;
  if (!base::StringToInt(hash_sv, &target_hash)) {
    return "";
  }
  return FindUrlForDomNodeId(last_page_content_->root_node(), target_hash);
}

std::optional<int32_t> PageContextMonitor::ResolveImageDomNodeId(
    std::string_view document_identifier,
    int32_t dom_node_id) const {
  if (!last_page_content_.has_value()) {
    return std::nullopt;
  }
  auto target_info = optimization_guide::FindNodeWithID(
      *last_page_content_, document_identifier, dom_node_id);
  if (!target_info || !target_info->node) {
    return std::nullopt;
  }
  const auto& node = *target_info->node;
  if (node.has_content_attributes() &&
      node.content_attributes().attribute_type() ==
          optimization_guide::proto::ContentAttributeType::
              CONTENT_ATTRIBUTE_IMAGE) {
    return node.content_attributes().common_ancestor_dom_node_id();
  }
  for (const auto& child : node.children_nodes()) {
    if (child.has_content_attributes() &&
        child.content_attributes().attribute_type() ==
            optimization_guide::proto::ContentAttributeType::
                CONTENT_ATTRIBUTE_IMAGE) {
      return child.content_attributes().common_ancestor_dom_node_id();
    }
  }
  return std::nullopt;
}

}  // namespace ttc
