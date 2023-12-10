// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/content/renderer/commerce_web_extractor.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace commerce {

namespace {
const char kOgContent[] = "content";
const char kOgPrefix[] = "og:";
const char kOgProperty[] = "property";
const char kPageMeta[] = "meta";
}  // namespace

CommerceWebExtractor::CommerceWebExtractor(
    content::RenderFrame* render_frame,
    service_manager::BinderRegistry* registry)
    : content::RenderFrameObserver(render_frame), render_frame_(render_frame) {
  registry->AddInterface(base::BindRepeating(
      &CommerceWebExtractor::BindReceiver, base::Unretained(this)));
}

CommerceWebExtractor::~CommerceWebExtractor() = default;

void CommerceWebExtractor::ExtractMetaInfo(ExtractMetaInfoCallback callback) {
  auto result = base::Value::Dict();
  blink::WebDocument doc = render_frame_->GetWebFrame()->GetDocument();
  blink::WebElementCollection collection =
      doc.GetElementsByHTMLTagName(kPageMeta);
  for (blink::WebElement element = collection.FirstItem(); !element.IsNull();
       element = collection.NextItem()) {
    if (!element.HasAttribute(kOgProperty) ||
        !element.HasAttribute(kOgContent)) {
      continue;
    }
    std::string name =
        base::UTF16ToUTF8(element.GetAttribute(kOgProperty).Utf16());
    std::string value =
        base::UTF16ToUTF8(element.GetAttribute(kOgContent).Utf16());
    if (base::StartsWith(name, kOgPrefix)) {
      result.Set(name.substr(3), value);
    }
  }
  std::move(callback).Run(base::Value(std::move(result)));
}

void CommerceWebExtractor::BindReceiver(
    mojo::PendingReceiver<commerce_web_extractor::mojom::CommerceWebExtractor>
        receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void CommerceWebExtractor::OnDestruct() {
  delete this;
}
}  // namespace commerce
