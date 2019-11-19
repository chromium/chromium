// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/clipboard/arc_clipboard_bridge.h"

#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/session/arc_bridge_service.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace arc {
namespace {

// Singleton factory for ArcClipboardBridge.
class ArcClipboardBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcClipboardBridge,
          ArcClipboardBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcClipboardBridgeFactory";

  static ArcClipboardBridgeFactory* GetInstance() {
    return base::Singleton<ArcClipboardBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcClipboardBridgeFactory>;
  ArcClipboardBridgeFactory() = default;
  ~ArcClipboardBridgeFactory() override = default;
};

mojom::ClipRepresentationPtr CreateHTML(const ui::Clipboard* clipboard) {
  DCHECK(clipboard);

  base::string16 markup16;
  // Unused. URL is sent from CreatePlainText() by reading it from the Bookmark.
  std::string url;
  uint32_t fragment_start, fragment_end;

  clipboard->ReadHTML(ui::ClipboardBuffer::kCopyPaste, &markup16, &url,
                      &fragment_start, &fragment_end);

  std::string text(base::UTF16ToUTF8(
      markup16.substr(fragment_start, fragment_end - fragment_start)));

  std::string mime_type(ui::kMimeTypeHTML);

  // Send non-sanitized HTML content. Instance should sanitize it if needed.
  return mojom::ClipRepresentation::New(mime_type,
                                        mojom::ClipValue::NewText(text));
}

mojom::ClipRepresentationPtr CreatePlainText(const ui::Clipboard* clipboard) {
  DCHECK(clipboard);

  // Unused. Title is not used at Instance.
  base::string16 title;
  std::string text;
  std::string mime_type(ui::kMimeTypeText);

  // Both Bookmark and AsciiText are represented by text/plain. If both are
  // present, only use Bookmark.
  clipboard->ReadBookmark(&title, &text);
  if (text.size() == 0)
    clipboard->ReadAsciiText(ui::ClipboardBuffer::kCopyPaste, &text);

  return mojom::ClipRepresentation::New(mime_type,
                                        mojom::ClipValue::NewText(text));
}

mojom::ClipDataPtr GetClipData(const ui::Clipboard* clipboard) {
  DCHECK(clipboard);

  std::vector<base::string16> mime_types;
  bool contains_files;
  clipboard->ReadAvailableTypes(ui::ClipboardBuffer::kCopyPaste, &mime_types,
                                &contains_files);

  mojom::ClipDataPtr clip_data(mojom::ClipData::New());

  // Populate ClipData with ClipRepresentation objects.
  for (const auto& mime_type16 : mime_types) {
    const std::string mime_type(base::UTF16ToUTF8(mime_type16));
    if (mime_type == ui::kMimeTypeHTML) {
      clip_data->representations.push_back(CreateHTML(clipboard));
    } else if (mime_type == ui::kMimeTypeText) {
      clip_data->representations.push_back(CreatePlainText(clipboard));
    } else {
      // TODO(ricardoq): Add other supported mime_types here.
      DLOG(WARNING) << "Unsupported mime type: " << mime_type;
    }
  }
  return clip_data;
}

void ProcessHTML(const mojom::ClipRepresentation* repr,
                 ui::ScopedClipboardWriter* writer) {
  DCHECK(repr);
  DCHECK(repr->value->is_text());
  DCHECK(writer);

  writer->WriteHTML(base::UTF8ToUTF16(repr->value->get_text()), std::string());
}

void ProcessPlainText(const mojom::ClipRepresentation* repr,
                      ui::ScopedClipboardWriter* writer) {
  DCHECK(repr);
  DCHECK(repr->value->is_text());
  DCHECK(writer);

  writer->WriteText(base::UTF8ToUTF16(repr->value->get_text()));
}

}  // namespace

// static
ArcClipboardBridge* ArcClipboardBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcClipboardBridgeFactory::GetForBrowserContext(context);
}

ArcClipboardBridge::ArcClipboardBridge(content::BrowserContext* context,
                                       ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      event_originated_at_instance_(false) {
  arc_bridge_service_->clipboard()->SetHost(this);
  ui::ClipboardMonitor::GetInstance()->AddObserver(this);
}

ArcClipboardBridge::~ArcClipboardBridge() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);
  arc_bridge_service_->clipboard()->SetHost(nullptr);
}

void ArcClipboardBridge::OnClipboardDataChanged() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (event_originated_at_instance_) {
    // Ignore this event, since this event was triggered by a 'copy' in
    // Instance, and not by Host.
    return;
  }

  mojom::ClipboardInstance* clipboard_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->clipboard(), OnHostClipboardUpdated);
  if (!clipboard_instance)
    return;

  // TODO(ricardoq): should only inform Instance when a supported mime_type is
  // copied to the clipboard.
  clipboard_instance->OnHostClipboardUpdated();
}

void ArcClipboardBridge::SetClipContent(mojom::ClipDataPtr clip_data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  if (!clipboard)
    return;

  // Order is important. AutoReset should outlive ScopedClipboardWriter.
  base::AutoReset<bool> auto_reset(&event_originated_at_instance_, true);
  ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);

  for (const auto& repr : clip_data->representations) {
    const std::string& mime_type(repr->mime_type);
    if (mime_type == ui::kMimeTypeHTML) {
      ProcessHTML(repr.get(), &writer);
    } else if (mime_type == ui::kMimeTypeText) {
      ProcessPlainText(repr.get(), &writer);
    }
  }
}

void ArcClipboardBridge::GetClipContent(GetClipContentCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  mojom::ClipDataPtr clip_data = GetClipData(clipboard);
  std::move(callback).Run(std::move(clip_data));
}

}  // namespace arc
