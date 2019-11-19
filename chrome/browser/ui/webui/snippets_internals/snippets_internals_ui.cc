// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/snippets_internals/snippets_internals_ui.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/snippets_internals/snippets_internals.mojom.h"
#include "chrome/browser/ui/webui/snippets_internals/snippets_internals_page_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#endif

SnippetsInternalsUI::SnippetsInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISnippetsInternalsHost);
  source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources 'self' 'unsafe-eval';");
  source->AddResourcePath("snippets_internals.css", IDR_SNIPPETS_INTERNALS_CSS);
  source->AddResourcePath("snippets_internals.js", IDR_SNIPPETS_INTERNALS_JS);
  source->AddResourcePath("snippets_internals.mojom-lite.js",
                          IDR_SNIPPETS_INTERNALS_MOJOM_LITE_JS);
  source->SetDefaultResource(IDR_SNIPPETS_INTERNALS_HTML);

    Profile* profile = Profile::FromWebUI(web_ui);
  content_suggestions_service_ =
      ContentSuggestionsServiceFactory::GetInstance()->GetForProfile(profile);
  pref_service_ = profile->GetPrefs();
  content::WebUIDataSource::Add(profile, source);
  AddHandlerToRegistry(base::BindRepeating(
      &SnippetsInternalsUI::BindSnippetsInternalsPageHandlerFactory,
      base::Unretained(this)));
}

SnippetsInternalsUI::~SnippetsInternalsUI() {}

void SnippetsInternalsUI::BindSnippetsInternalsPageHandlerFactory(
    mojo::PendingReceiver<snippets_internals::mojom::PageHandlerFactory>
        receiver) {
  receiver_.reset();

  receiver_.Bind(std::move(receiver));
}

void SnippetsInternalsUI::CreatePageHandler(
    mojo::PendingRemote<snippets_internals::mojom::Page> page,
    CreatePageHandlerCallback callback) {
  DCHECK(page);
  mojo::PendingRemote<snippets_internals::mojom::PageHandler> handler;
  page_handler_ = std::make_unique<SnippetsInternalsPageHandler>(
      handler.InitWithNewPipeAndPassReceiver(), std::move(page),
      content_suggestions_service_, pref_service_);

  std::move(callback).Run(std::move(handler));
}
