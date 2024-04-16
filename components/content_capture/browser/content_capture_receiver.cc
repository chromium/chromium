// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/browser/content_capture_receiver.h"

#include <utility>

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/content_capture/browser/onscreen_content_provider.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"

namespace content_capture {

namespace {

OnscreenContentProvider* GetOnscreenContentProvider(
    content::RenderFrameHost* rfh) {
  if (auto* web_contents = content::WebContents::FromRenderFrameHost(rfh))
    return OnscreenContentProvider::FromWebContents(web_contents);
  return nullptr;
}

std::string ToFaviconTypeString(blink::mojom::FaviconIconType type) {
  if (type == blink::mojom::FaviconIconType::kFavicon)
    return "favicon";
  else if (type == blink::mojom::FaviconIconType::kTouchIcon)
    return "touch icon";
  else if (type == blink::mojom::FaviconIconType::kTouchPrecomposedIcon)
    return "touch precomposed icon";
  return "invalid";
}

}  // namespace

// static
std::string ContentCaptureReceiver::ToJSON(
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  if (candidates.empty())
    return std::string();
  base::Value::List favicon_array;
  for (const auto& favicon_url : candidates) {
    base::Value::Dict favicon;
    favicon.Set("url", favicon_url->icon_url.spec());
    favicon.Set("type", ToFaviconTypeString(favicon_url->icon_type));

    if (!favicon_url->icon_sizes.empty()) {
      base::Value::List sizes;
      for (auto icon_size : favicon_url->icon_sizes) {
        base::Value::Dict size;
        size.Set("width", icon_size.width());
        size.Set("height", icon_size.height());
        sizes.Append(std::move(size));
      }
      favicon.Set("sizes", std::move(sizes));
    }
    favicon_array.Append(std::move(favicon));
  }
  std::string result;
  base::JSONWriter::Write(favicon_array, &result);
  return result;
}

ContentCaptureReceiver::ContentCaptureReceiver(content::RenderFrameHost* rfh)
    : rfh_(rfh), id_(GetIdFrom(rfh)) {}

ContentCaptureReceiver::~ContentCaptureReceiver() = default;

int64_t ContentCaptureReceiver::GetIdFrom(content::RenderFrameHost* rfh) {
  return static_cast<int64_t>(rfh->GetProcess()->GetID()) << 32 |
         (rfh->GetRoutingID() & 0xFFFFFFFF);
}

void ContentCaptureReceiver::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::ContentCaptureReceiver>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void ContentCaptureReceiver::DidCaptureContent(const ContentCaptureData& data,
                                               bool first_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* provider = GetOnscreenContentProvider(rfh_);
  if (!provider)
    return;

  if (first_data) {
    // The session id of this frame isn't changed for new document navigation,
    // so the previous session should be terminated.
    // The parent frame might be captured after child, we need to check if url
    // is changed, otherwise the child frame's session will be removed.
    if (frame_content_capture_data_.id != 0 &&
        frame_content_capture_data_.url != data.value) {
      RemoveSession();
    }

    frame_content_capture_data_.id = id_;
    // Copies everything except id and children.
    frame_content_capture_data_.url = data.value;
    frame_content_capture_data_.bounds = data.bounds;
    RetrieveFaviconURL();

    has_session_ = true;
  }
  // We can't avoid copy the data here because frame needs to be replaced.
  // Always have frame URL attached, since the ContentCaptureConsumer will
  // be reset once activity is resumed, URL is needed to rebuild session.
  ContentCaptureFrame frame(frame_content_capture_data_);
  frame.children = data.children;
  provider->DidCaptureContent(this, frame);
}

void ContentCaptureReceiver::DidUpdateContent(const ContentCaptureData& data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* provider = GetOnscreenContentProvider(rfh_);
  if (!provider)
    return;

  // We can't avoid copy the data here because frame needs to be replaced.
  ContentCaptureFrame frame(frame_content_capture_data_);
  frame.children = data.children;
  provider->DidUpdateContent(this, frame);
}

void ContentCaptureReceiver::DidRemoveContent(
    const std::vector<int64_t>& data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* provider = GetOnscreenContentProvider(rfh_);
  if (!provider)
    return;
  provider->DidRemoveContent(this, data);
}

void ContentCaptureReceiver::StartCapture() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (content_capture_enabled_)
    return;

  if (auto& sender = GetContentCaptureSender()) {
    sender->StartCapture();
    content_capture_enabled_ = true;
  }
}

void ContentCaptureReceiver::StopCapture() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!content_capture_enabled_)
    return;

  if (auto& sender = GetContentCaptureSender()) {
    sender->StopCapture();
    content_capture_enabled_ = false;
  }
}

void ContentCaptureReceiver::RemoveSession() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!has_session_)
    return;

  // TODO(crbug.com/40641263): Find a way to notify of session being removed if
  // rfh isn't available.
  if (auto* provider = GetOnscreenContentProvider(rfh_)) {
    provider->DidRemoveSession(this);
    has_session_ = false;
    // We can't reset the frame_content_capture_data_ here, because it could be
    // used by GetFrameContentCaptureDataLastSeen(), has_session_ is used to
    // check if new session shall be created as needed.
  }

  // Cancel the task if any.
  if (notify_title_update_callback_) {
    notify_title_update_callback_->Cancel();
    notify_title_update_callback_ = nullptr;
    title_update_task_runner_ = nullptr;
  }
  exponential_delay_ = 1;
}

void ContentCaptureReceiver::SetTitle(const std::u16string& title) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  frame_content_capture_data_.title = title;
  if (!has_session_)
    return;

  // Returns if there is the pending task.
  if (notify_title_update_callback_)
    return;

  notify_title_update_callback_ =
      std::make_unique<base::CancelableOnceClosure>(base::BindOnce(
          &ContentCaptureReceiver::NotifyTitleUpdate, base::Unretained(this)));

  if (!title_update_task_runner_)
    title_update_task_runner_ = content::GetUIThreadTaskRunner({});

  title_update_task_runner_->PostDelayedTask(
      FROM_HERE, notify_title_update_callback_->callback(),
      base::Seconds(exponential_delay_));

  exponential_delay_ =
      exponential_delay_ < 256 ? exponential_delay_ * 2 : exponential_delay_;
}

void ContentCaptureReceiver::UpdateFaviconURL(
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  if (!has_session_)
    return;
  frame_content_capture_data_.favicon = ToJSON(candidates);
  auto* provider = GetOnscreenContentProvider(rfh_);
  if (!provider)
    return;
  provider->DidUpdateFavicon(this);
}

void ContentCaptureReceiver::RetrieveFaviconURL() {
  if (!rfh()->IsActive() || !rfh()->IsInPrimaryMainFrame() ||
      disable_get_favicon_from_web_contents_for_testing()) {
    frame_content_capture_data_.favicon = std::string();
  } else {
    frame_content_capture_data_.favicon = ToJSON(rfh()->FaviconURLs());
  }
}

void ContentCaptureReceiver::NotifyTitleUpdate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (auto* provider = GetOnscreenContentProvider(rfh_))
    provider->DidUpdateTitle(this);

  // Reset the task after running.
  notify_title_update_callback_ = nullptr;
  title_update_task_runner_ = nullptr;
}

const mojo::AssociatedRemote<mojom::ContentCaptureSender>&
ContentCaptureReceiver::GetContentCaptureSender() {
  if (!content_capture_sender_) {
    rfh_->GetRemoteAssociatedInterfaces()->GetInterface(
        &content_capture_sender_);
  }
  return content_capture_sender_;
}

const ContentCaptureFrame& ContentCaptureReceiver::GetContentCaptureFrame() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::u16string url = base::UTF8ToUTF16(rfh_->GetLastCommittedURL().spec());
  if (url == frame_content_capture_data_.url && has_session_)
    return frame_content_capture_data_;

  if (frame_content_capture_data_.id != 0 && has_session_)
    RemoveSession();

  frame_content_capture_data_.id = id_;
  frame_content_capture_data_.url = url;
  const std::optional<gfx::Size>& size = rfh_->GetFrameSize();
  if (size.has_value())
    frame_content_capture_data_.bounds = gfx::Rect(size.value());
  RetrieveFaviconURL();

  has_session_ = true;
  return frame_content_capture_data_;
}

// static
bool
    ContentCaptureReceiver::disable_get_favicon_from_web_contents_for_testing_ =
        false;

void ContentCaptureReceiver::DisableGetFaviconFromWebContentsForTesting() {
  disable_get_favicon_from_web_contents_for_testing_ = true;
}

// static
bool ContentCaptureReceiver::
    disable_get_favicon_from_web_contents_for_testing() {
  return disable_get_favicon_from_web_contents_for_testing_;
}

}  // namespace content_capture
