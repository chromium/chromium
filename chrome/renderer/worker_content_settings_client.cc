// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/worker_content_settings_client.h"

#include "base/memory/ptr_util.h"
#include "components/content_settings/renderer/content_settings_agent_impl.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/origin.h"

WorkerContentSettingsClient::WorkerContentSettingsClient(
    content::RenderFrame* render_frame)
    : frame_token_(render_frame->GetWebFrame()->GetLocalFrameToken()) {
  blink::WebLocalFrame* frame = render_frame->GetWebFrame();
  const blink::WebDocument& document = frame->GetDocument();
  if (document.GetSecurityOrigin().IsOpaque() ||
      frame->Top()->GetSecurityOrigin().IsOpaque())
    is_unique_origin_ = true;

  document_origin_ = document.GetSecurityOrigin();
  site_for_cookies_ = document.SiteForCookies();
  top_frame_origin_ = document.TopFrameOrigin();

  content::ChildThread::Get()->BindHostReceiver(
      pending_content_settings_manager_.InitWithNewPipeAndPassReceiver());

  content_settings::ContentSettingsAgentImpl* agent =
      content_settings::ContentSettingsAgentImpl::Get(render_frame);
  allow_running_insecure_content_ = agent->allow_running_insecure_content();
  RendererContentSettingRules* rules = agent->GetRendererContentSettingRules();
  if (rules) {
    // Note: Makes a copy of the rules instead of directly using a pointer as
    // there is no guarantee that the RenderFrame will exist throughout this
    // object's lifetime.
    content_setting_rules_ =
        std::make_unique<RendererContentSettingRules>(*rules);
  }
}

WorkerContentSettingsClient::WorkerContentSettingsClient(
    const WorkerContentSettingsClient& other)
    : is_unique_origin_(other.is_unique_origin_),
      document_origin_(other.document_origin_),
      site_for_cookies_(other.site_for_cookies_),
      top_frame_origin_(other.top_frame_origin_),
      allow_running_insecure_content_(other.allow_running_insecure_content_),
      frame_token_(other.frame_token_) {
  other.EnsureContentSettingsManager();
  other.content_settings_manager_->Clone(
      pending_content_settings_manager_.InitWithNewPipeAndPassReceiver());
  if (other.content_setting_rules_)
    content_setting_rules_ = std::make_unique<RendererContentSettingRules>(
        *(other.content_setting_rules_));
}

WorkerContentSettingsClient::~WorkerContentSettingsClient() {}

std::unique_ptr<blink::WebContentSettingsClient>
WorkerContentSettingsClient::Clone() {
  return base::WrapUnique(new WorkerContentSettingsClient(*this));
}

void WorkerContentSettingsClient::AllowStorageAccess(
    StorageType storage_type,
    base::OnceCallback<void(bool)> callback) {
  if (is_unique_origin_) {
    std::move(callback).Run(false);
    return;
  }
  EnsureContentSettingsManager();

  content_settings_manager_->AllowStorageAccess(
      frame_token_,
      content_settings::ContentSettingsAgentImpl::ConvertToMojoStorageType(
          storage_type),
      document_origin_, site_for_cookies_, top_frame_origin_,
      std::move(callback));
}

bool WorkerContentSettingsClient::AllowStorageAccessSync(
    StorageType storage_type) {
  if (is_unique_origin_)
    return false;

  EnsureContentSettingsManager();

  bool result = false;
  content_settings_manager_->AllowStorageAccess(
      frame_token_,
      content_settings::ContentSettingsAgentImpl::ConvertToMojoStorageType(
          storage_type),
      document_origin_, site_for_cookies_, top_frame_origin_, &result);
  return result;
}

bool WorkerContentSettingsClient::AllowRunningInsecureContent(
    bool allowed_per_settings,
    const blink::WebURL& url) {
  if (!allow_running_insecure_content_ && !allowed_per_settings) {
    EnsureContentSettingsManager();
    content_settings_manager_->OnContentBlocked(
        frame_token_, ContentSettingsType::MIXEDSCRIPT);
    return false;
  }

  return true;
}

bool WorkerContentSettingsClient::ShouldAutoupgradeMixedContent() {
  if (content_setting_rules_) {
    if (content_setting_rules_->mixed_content_rules.size() > 0)
      return content_setting_rules_->mixed_content_rules[0]
                 .GetContentSetting() != CONTENT_SETTING_ALLOW;
  }
  return false;
}

void WorkerContentSettingsClient::EnsureContentSettingsManager() const {
  // Lazily bind `content_settings_manager_` so it is bound on the right thread.
  if (content_settings_manager_)
    return;
  DCHECK(pending_content_settings_manager_);
  content_settings_manager_.Bind(std::move(pending_content_settings_manager_));
}
