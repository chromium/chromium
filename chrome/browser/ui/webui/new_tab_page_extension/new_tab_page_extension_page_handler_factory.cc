// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page_extension/new_tab_page_extension_page_handler_factory.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/search_provider_logos/logo_service_factory.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_generator.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"
#include "chrome/browser/ui/webui/searchbox/realbox_handler.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

DOCUMENT_USER_DATA_KEY_IMPL(NewTabPageExtensionPageHandlerFactory);

NewTabPageExtensionPageHandlerFactory::NewTabPageExtensionPageHandlerFactory(
    content::RenderFrameHost* rfh)
    : content::DocumentUserData<NewTabPageExtensionPageHandlerFactory>(rfh),
      navigation_start_time_(base::Time::Now()),
      navigation_start_time_ticks_(base::TimeTicks::Now()) {}

NewTabPageExtensionPageHandlerFactory::
    ~NewTabPageExtensionPageHandlerFactory() = default;

void NewTabPageExtensionPageHandlerFactory::Bind(
    mojo::PendingReceiver<new_tab_page::mojom::PageHandlerFactory> receiver) {
  page_receiver_.Bind(std::move(receiver));
}

void NewTabPageExtensionPageHandlerFactory::Bind(
    mojo::PendingReceiver<searchbox::mojom::PageHandlerFactory> receiver) {
  searchbox_receiver_.Bind(std::move(receiver));
}

void NewTabPageExtensionPageHandlerFactory::Bind(
    mojo::PendingReceiver<composebox::mojom::PageHandlerFactory> receiver) {
  composebox_receiver_.Bind(std::move(receiver));
}

void NewTabPageExtensionPageHandlerFactory::Bind(
    mojo::PendingReceiver<action_chips::mojom::ActionChipsHandlerFactory>
        receiver) {
  action_chips_receiver_.Bind(std::move(receiver));
}

void NewTabPageExtensionPageHandlerFactory::CreatePageHandler(
    mojo::PendingRemote<new_tab_page::mojom::Page> page,
    mojo::PendingReceiver<new_tab_page::mojom::PageHandler> handler) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  page_handler_ = std::make_unique<NewTabPageHandler>(
      std::move(handler), std::move(page), profile,
      NtpCustomBackgroundServiceFactory::GetForProfile(profile),
      ThemeServiceFactory::GetForProfile(profile),
      LogoServiceFactory::GetForProfile(profile),
      SyncServiceFactory::GetForProfile(profile),
      segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
          profile),
      web_contents, navigation_start_time_, navigation_start_time_ticks_,
      &module_id_details_);
}

void NewTabPageExtensionPageHandlerFactory::CreatePageHandler(
    mojo::PendingRemote<searchbox::mojom::Page> page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler> handler) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  // Unretained is safe: `this` owns `realbox_handler_`.
  realbox_handler_ = std::make_unique<RealboxHandler>(
      std::move(handler), std::move(page), profile, web_contents,
      base::BindRepeating(&NewTabPageExtensionPageHandlerFactory::
                              GetOrCreateContextualSessionHandle,
                          base::Unretained(this)));
}

void NewTabPageExtensionPageHandlerFactory::CreatePageHandler(
    mojo::PendingReceiver<composebox::mojom::PageHandler> handler,
    mojo::PendingRemote<searchbox::mojom::Page> searchbox_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler> searchbox_handler) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  // Unretained is safe: `this` owns `composebox_handler_`.
  composebox_handler_ = std::make_unique<ComposeboxHandler>(
      std::move(handler), std::move(searchbox_handler),
      std::move(searchbox_page), profile, web_contents,
      base::BindRepeating(&NewTabPageExtensionPageHandlerFactory::
                              GetOrCreateContextualSessionHandle,
                          base::Unretained(this)),
      base::BindRepeating(
          &NewTabPageExtensionPageHandlerFactory::ClearContextualSessionHandle,
          base::Unretained(this)));
}

void NewTabPageExtensionPageHandlerFactory::CreateActionChipsHandler(
    mojo::PendingReceiver<action_chips::mojom::ActionChipsHandler> handler,
    mojo::PendingRemote<action_chips::mojom::Page> page) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  // Unretained is safe: `this` owns `action_chips_handler_`.
  action_chips_handler_ = std::make_unique<ActionChipsHandler>(
      std::move(handler), std::move(page), profile, web_contents,
      std::make_unique<ActionChipsGeneratorImpl>(profile),
      base::BindRepeating(&NewTabPageExtensionPageHandlerFactory::
                              GetOrCreateContextualSessionHandle,
                          base::Unretained(this)));
}

contextual_search::ContextualSearchSessionHandle*
NewTabPageExtensionPageHandlerFactory::GetOrCreateContextualSessionHandle() {
  if (!shared_session_handle_) {
    Profile* profile =
        Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
    auto* contextual_search_service =
        ContextualSearchServiceFactory::GetForProfile(profile);
    if (contextual_search_service) {
      shared_session_handle_ = contextual_search_service->CreateSession(
          ntp_composebox::CreateQueryControllerConfigParams(),
          contextual_search::ContextualSearchSource::kNewTabPage,
          lens::LensOverlayInvocationSource::kNtpContextualQuery);
      shared_session_handle_->CheckSearchContentSharingSettings(
          profile->GetPrefs());
    }
  }
  return shared_session_handle_.get();
}

void NewTabPageExtensionPageHandlerFactory::ClearContextualSessionHandle() {
  shared_session_handle_.reset();
}
