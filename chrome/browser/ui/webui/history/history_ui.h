// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_UI_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "components/user_education/webui/user_education.mojom.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"
#include "ui/webui/resources/cr_components/history/foreign_sessions.mojom.h"
#include "ui/webui/resources/cr_components/history/history.mojom-forward.h"
#include "ui/webui/resources/cr_components/history/history_cross_device_signin_promo.mojom.h"

#if !BUILDFLAG(IS_ANDROID)
#include "ui/webui/resources/cr_components/history_clusters/history_clusters.mojom.h"
#include "ui/webui/resources/cr_components/history_embeddings/history_embeddings.mojom.h"  // nogncheck
#endif  // !BUILDFLAG(IS_ANDROID)

namespace base {
class RefCountedMemory;
}

class BrowsingHistoryHandler;

#if !BUILDFLAG(IS_ANDROID)
namespace history_clusters {
class HistoryClustersHandler;
}

class HistoryEmbeddingsHandler;
#endif  // !BUILDFLAG(IS_ANDROID)

namespace page_image_service {
class ImageServiceHandler;
}

class HistoryUIConfig : public content::WebUIConfig {
 public:
  HistoryUIConfig();
  ~HistoryUIConfig() override;

  // content::WebUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

class HistoryUI
    : public ui::MojoWebUIController,
      public help_bubble::mojom::HelpBubbleHandlerFactory,
#if !BUILDFLAG(IS_ANDROID)
      public history_embeddings::mojom::PageHandlerFactory,
      public history_clusters::mojom::PageHandlerFactory,
#endif  // !BUILDFLAG(IS_ANDROID)
      public user_education::mojom::UserEducationMixedTrustHandlerFactory,
      public history::mojom::ForeignSessionPageHandlerFactory {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kHistoryGeminiFilterChipElementId);

  explicit HistoryUI(content::WebUI* web_ui);
  HistoryUI(const HistoryUI&) = delete;
  HistoryUI& operator=(const HistoryUI&) = delete;
  ~HistoryUI() override;

  static scoped_refptr<base::RefCountedMemory> GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);

  void BindInterface(
      mojo::PendingReceiver<history::mojom::PageHandler> pending_page_handler);
  void BindInterface(
      mojo::PendingReceiver<page_image_service::mojom::PageImageServiceHandler>
          pending_page_handler);
  void BindInterface(
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
          pending_receiver);
  void BindInterface(
      mojo::PendingReceiver<history::mojom::ForeignSessionPageHandlerFactory>
          pending_receiver);
  void BindInterface(
      mojo::PendingReceiver<
          user_education::mojom::UserEducationMixedTrustHandlerFactory>
          pending_receiver);
  void BindInterface(
      mojo::PendingReceiver<history_cross_device_signin_promo::mojom::
                                HistoryCrossDeviceSigninPromoHandler>
          pending_receiver);

#if !BUILDFLAG(IS_ANDROID)
  void BindInterface(
      mojo::PendingReceiver<history_embeddings::mojom::PageHandlerFactory>
          pending_page_handler_factory);
  void BindInterface(
      mojo::PendingReceiver<history_clusters::mojom::PageHandlerFactory>
          pending_page_handler_factory);

  // history_embeddings::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<history_embeddings::mojom::Page> page,
      mojo::PendingReceiver<history_embeddings::mojom::PageHandler> receiver)
      override;

  // history_clusters::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<history_clusters::mojom::Page> page,
      mojo::PendingReceiver<history_clusters::mojom::PageHandler> receiver)
      override;
#endif  // !BUILDFLAG(IS_ANDROID)

  // help_bubble::mojom::HelpBubbleHandlerFactory:
  void CreateHelpBubbleHandler(
      mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler)
      override;

  // history::mojom::ForeignSessionPageHandlerFactory:
  void CreateForeignSessionPageHandler(
      mojo::PendingRemote<history::mojom::ForeignSessionPage> page,
      mojo::PendingReceiver<history::mojom::ForeignSessionPageHandler> receiver)
      override;

  // user_education::mojom::UserEducationMixedTrustHandlerFactory:
  void CreateUserEducationMixedTrustHandler(
      mojo::PendingReceiver<
          user_education::mojom::UserEducationMixedTrustHandler> receiver)
      override;

  // For testing only:
  BrowsingHistoryHandler* GetBrowsingHistoryHandlerForTesting() {
    return browsing_history_handler_.get();
  }
#if !BUILDFLAG(IS_ANDROID)
  history_clusters::HistoryClustersHandler*
  GetHistoryClustersHandlerForTesting() {
    return history_clusters_handler_.get();
  }
#endif  // !BUILDFLAG(IS_ANDROID)

 private:
  void UpdateDataSource();

  std::unique_ptr<BrowsingHistoryHandler> browsing_history_handler_;
  std::unique_ptr<page_image_service::ImageServiceHandler>
      image_service_handler_;
  std::unique_ptr<user_education::HelpBubbleHandler> help_bubble_handler_;
  std::unique_ptr<history::mojom::ForeignSessionPageHandler>
      foreign_session_handler_;
  std::unique_ptr<user_education::mojom::UserEducationMixedTrustHandler>
      user_education_handler_;
  std::unique_ptr<history_cross_device_signin_promo::mojom::
                      HistoryCrossDeviceSigninPromoHandler>
      history_cross_device_signin_promo_handler_;

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<HistoryEmbeddingsHandler> history_embeddings_handler_;
  std::unique_ptr<history_clusters::HistoryClustersHandler>
      history_clusters_handler_;
  mojo::Receiver<history_embeddings::mojom::PageHandlerFactory>
      history_embeddings_handler_factory_receiver_{this};
  mojo::Receiver<history_clusters::mojom::PageHandlerFactory>
      history_clusters_handler_factory_receiver_{this};
#endif  // !BUILDFLAG(IS_ANDROID)

  PrefChangeRegistrar pref_change_registrar_;

  mojo::Receiver<help_bubble::mojom::HelpBubbleHandlerFactory>
      help_bubble_handler_factory_receiver_{this};
  mojo::Receiver<user_education::mojom::UserEducationMixedTrustHandlerFactory>
      user_education_handler_factory_receiver_{this};
  mojo::Receiver<history::mojom::ForeignSessionPageHandlerFactory>
      foreign_session_page_handler_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_UI_H_
