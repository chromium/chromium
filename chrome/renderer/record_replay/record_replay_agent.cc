// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/record_replay/record_replay_agent.h"

#include <string>

#include "base/containers/to_vector.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/common/record_replay/aliases.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_dom_event.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"

namespace record_replay {

namespace {

std::string BuildElementSelector(const blink::WebElement& element) {
  std::string id = element.GetIdAttribute().Utf8();
  if (!id.empty()) {
    if (!base::IsAsciiAlpha(id.front()) ||
        std::ranges::all_of(id, &base::IsAsciiDigit<char>)) {
      return base::StrCat({"[id=\"", id, "\"]"});
    }
    return base::StrCat({"#", id});
  }
  if (auto parent = element.ParentNode().DynamicTo<blink::WebElement>()) {
    int index = 0;
    for (blink::WebNode n = element; n; n = n.PreviousSibling()) {
      if (blink::WebElement e = n.DynamicTo<blink::WebElement>()) {
        ++index;
      }
    }
    return base::StrCat({BuildElementSelector(parent), " > ",
                         element.TagName().Utf8(), ":nth-child(",
                         base::NumberToString(index), ")"});
  }
  return ":root";
}

}  // namespace

RecordReplayAgent::RecordReplayAgent(
    content::RenderFrame* render_frame,
    blink::AssociatedInterfaceRegistry* registry)
    : content::RenderFrameObserver(render_frame) {
  render_frame->GetWebFrame()->SetRecordReplayClient(this);
  registry->AddInterface<mojom::RecordReplayAgent>(base::BindRepeating(
      &RecordReplayAgent::BindPendingReceiver, base::Unretained(this)));
}

RecordReplayAgent::~RecordReplayAgent() = default;

void RecordReplayAgent::WillDetach(blink::DetachReason detach_reason) {
  if (auto* frame = render_frame()) {
    frame->GetWebFrame()->SetRecordReplayClient(this);
  }
}

// Destroys itself asynchronously because OnDestruct() can be triggered
// synchronously by JavaScript, and that JavaScript might be triggered
// synchronously by `this`.
void RecordReplayAgent::OnDestruct() {
  receiver_.reset();
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

blink::WebDocument RecordReplayAgent::GetDocument() {
  return render_frame() ? render_frame()->GetWebFrame()->GetDocument()
                        : blink::WebDocument();
}

void RecordReplayAgent::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::RecordReplayAgent>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

mojo::AssociatedRemote<mojom::RecordReplayDriver>&
RecordReplayAgent::GetDriver() {
  if (!driver_) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(&driver_);
  }
  return driver_;
}

void RecordReplayAgent::StartRecording() {
  record_ = true;
}

void RecordReplayAgent::StopRecording() {
  record_ = false;
}

void RecordReplayAgent::GetElementSelector(
    DomNodeId dom_node_id,
    base::OnceCallback<void(Selector)> cb) {
  blink::WebElement element = blink::WebNode::FromDomNodeId(*dom_node_id)
                                  .DynamicTo<blink::WebElement>();
  if (!element) {
    std::move(cb).Run(Selector(""));
    return;
  }
  std::move(cb).Run(Selector(BuildElementSelector(element)));
}

void RecordReplayAgent::GetMatchingElements(
    Selector element_selector,
    base::OnceCallback<void(const std::vector<DomNodeId>&)> cb) {
  const blink::WebDocument document = GetDocument();
  if (!document) {
    std::move(cb).Run({});
    return;
  }
  const std::vector<blink::WebElement> matches =
      document.QuerySelectorAll(blink::WebString::FromUTF8(*element_selector));
  std::move(cb).Run(
      base::ToVector(matches, [](const blink::WebElement& e) -> DomNodeId {
        return DomNodeId(e.GetDomNodeId());
      }));
}

void RecordReplayAgent::DoClick(DomNodeId dom_node_id,
                                base::OnceCallback<void(bool)> cb) {
  blink::WebElement element = blink::WebNode::FromDomNodeId(*dom_node_id)
                                  .DynamicTo<blink::WebElement>();
  if (!element) {
    std::move(cb).Run(false);
    return;
  }
  element.Click();
  std::move(cb).Run(true);
}

void RecordReplayAgent::DoPaste(DomNodeId dom_node_id,
                                FieldValue text,
                                base::OnceCallback<void(bool)> cb) {
  blink::WebFormControlElement element =
      blink::WebNode::FromDomNodeId(*dom_node_id)
          .DynamicTo<blink::WebFormControlElement>();
  if (!element) {
    std::move(cb).Run(false);
    return;
  }
  element.PasteText(blink::WebString::FromUTF8(*text), /*replace_all=*/false);
  std::move(cb).Run(true);
}

void RecordReplayAgent::DoSelect(DomNodeId dom_node_id,
                                 FieldValue value,
                                 base::OnceCallback<void(bool)> cb) {
  blink::WebFormControlElement element =
      blink::WebNode::FromDomNodeId(*dom_node_id)
          .DynamicTo<blink::WebFormControlElement>();
  if (!element ||
      element.FormControlType() != blink::mojom::FormControlType::kSelectOne) {
    std::move(cb).Run(false);
    return;
  }
  element.SetValue(blink::WebString::FromUTF8(*value), /*send_events=*/true);
  std::move(cb).Run(true);
}

void RecordReplayAgent::DidReceiveLeftMouseDownOrGestureTapInNode(
    const blink::WebNode& node) {
  if (!record_) {
    return;
  }

  if (node.IsNull()) {
    return;
  }

  blink::WebNode current_node = node;
  while (!current_node.IsNull() && !current_node.IsElementNode()) {
    current_node = current_node.ParentNode();
  }

  if (current_node.IsNull()) {
    return;
  }

  blink::WebElement element = current_node.DynamicTo<blink::WebElement>();
  if (!element) {
    return;
  }

  GetDriver()->OnClick(DomNodeId(element.GetDomNodeId()),
                       Selector(BuildElementSelector(element)));
}

void RecordReplayAgent::SelectControlSelectionChanged(
    const blink::WebFormControlElement& element) {
  if (!record_) {
    return;
  }
  GetDriver()->OnSelectChanged(DomNodeId(element.GetDomNodeId()),
                               Selector(BuildElementSelector(element)),
                               FieldValue(element.Value().Utf8()));
}

void RecordReplayAgent::TextFieldDidEndEditing(
    const blink::WebInputElement& element) {
  if (!record_) {
    return;
  }
  GetDriver()->OnTextChange(DomNodeId(element.GetDomNodeId()),
                            Selector(BuildElementSelector(element)),
                            FieldValue(element.Value().Utf8()));
}

}  // namespace record_replay
