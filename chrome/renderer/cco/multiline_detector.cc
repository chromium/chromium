// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/cco/multiline_detector.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/chrome_features.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_element.h"

namespace {

bool IsContentEditable(const blink::WebElement& element) {
  if (element.IsNull() || !element.IsEditable() ||
      !element.HasAttribute("contenteditable")) {
    return false;
  }

  const auto content_editable = element.GetAttribute("contenteditable");
  return content_editable.IsEmpty() ||
         (content_editable.ContainsOnlyASCII() &&
          base::EqualsCaseInsensitiveASCII(content_editable.Ascii(), "true"));
}

}  // namespace

// static
void MultilineDetector::InstallIfNecessary(content::RenderFrame* render_frame) {
  if (base::FeatureList::IsEnabled(features::kCcoTest1)) {
    // MultilineDetector deletes itself when the frame is destroyed.
    new MultilineDetector(render_frame);
  }
}

MultilineDetector::MultilineDetector(content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {}

MultilineDetector::~MultilineDetector() = default;

void MultilineDetector::OnDestruct() {
  delete this;
}

void MultilineDetector::FocusedElementChanged(
    const blink::WebElement& element) {
  if (IsContentEditable(element)) {
    const blink::WebString tag = element.TagName();
    const blink::WebString role = element.GetAttribute("role");
    const blink::WebString multiline = element.GetAttribute("aria-multiline");
    const blink::WebString placeholder =
        element.GetAttribute("aria-placeholder");
    const blink::WebString label = element.GetAttribute("aria-label");
    const blink::WebString labelled_by =
        element.GetAttribute("aria-labelledby");
    const blink::WebString described_by =
        element.GetAttribute("aria-describedby");
    render_frame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kInfo,
        base::StringPrintf(
            "Focused content-editable "
            "element\ntag=%s\nrole=%s\nmultiline=%s\nplaceholder=%s\nlabel=%"
            "s\nlabelled-by=%s\ndescribed-by=%s",
            tag.Utf8().c_str(), role.Utf8().c_str(), multiline.Utf8().c_str(),
            placeholder.Utf8().c_str(), label.Utf8().c_str(),
            labelled_by.Utf8().c_str(), described_by.Utf8().c_str()));
  }
}
