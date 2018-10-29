// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/browser/autofill_driver_ios.h"

#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/ios/browser/autofill_driver_ios_bridge.h"
#include "components/autofill/ios/browser/autofill_driver_ios_webframe.h"
#include "components/autofill/ios/browser/autofill_driver_ios_webstate.h"
#include "components/autofill/ios/browser/autofill_switches.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/origin_util.h"
#import "ios/web/public/web_state/web_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "ui/gfx/geometry/rect_f.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

// static
void AutofillDriverIOS::PrepareForWebStateWebFrameAndDelegate(
    web::WebState* web_state,
    AutofillClient* client,
    id<AutofillDriverIOSBridge> bridge,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager) {
  if (autofill::switches::IsAutofillIFrameMessagingEnabled()) {
    AutofillDriverIOSWebFrameFactory::CreateForWebStateAndDelegate(
        web_state, client, bridge, app_locale, enable_download_manager);
    // By the time this method is called, no web_frame is available. This method
    // only prepare the factory and the AutofillDriverIOS will be created in the
    // first call to FromWebStateAndWebFrame.
  } else {
    AutofillDriverIOSWebState::CreateForWebStateAndDelegate(
        web_state, client, bridge, app_locale, enable_download_manager);
  }
}

// static
AutofillDriverIOS* AutofillDriverIOS::FromWebStateAndWebFrame(
    web::WebState* web_state,
    web::WebFrame* web_frame) {
  if (autofill::switches::IsAutofillIFrameMessagingEnabled()) {
    return AutofillDriverIOSWebFrameFactory::FromWebState(web_state)
        ->AutofillDriverIOSFromWebFrame(web_frame)
        ->driver();
  } else {
    return AutofillDriverIOSWebState::FromWebState(web_state);
  }
}

AutofillDriverIOS::AutofillDriverIOS(
    web::WebState* web_state,
    web::WebFrame* web_frame,
    AutofillClient* client,
    id<AutofillDriverIOSBridge> bridge,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager)
    : web_state_(web_state),
      web_frame_(web_frame),
      bridge_(bridge),
      autofill_manager_(this, client, app_locale, enable_download_manager),
      autofill_external_delegate_(&autofill_manager_, this) {
  autofill_manager_.SetExternalDelegate(&autofill_external_delegate_);
}

AutofillDriverIOS::~AutofillDriverIOS() {}

bool AutofillDriverIOS::IsIncognito() const {
  return web_state_->GetBrowserState()->IsOffTheRecord();
}

bool AutofillDriverIOS::IsInMainFrame() const {
  return web_frame_ ? web_frame_->IsMainFrame() : true;
}

net::URLRequestContextGetter* AutofillDriverIOS::GetURLRequestContext() {
  return web_state_->GetBrowserState()->GetRequestContext();
}

scoped_refptr<network::SharedURLLoaderFactory>
AutofillDriverIOS::GetURLLoaderFactory() {
  return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
      web_state_->GetBrowserState()->GetURLLoaderFactory());
}

bool AutofillDriverIOS::RendererIsAvailable() {
  return true;
}

void AutofillDriverIOS::SendFormDataToRenderer(
    int query_id,
    RendererFormDataAction action,
    const FormData& data) {
  [bridge_ onFormDataFilled:query_id inFrame:web_frame_ result:data];
}

void AutofillDriverIOS::PropagateAutofillPredictions(
    const std::vector<autofill::FormStructure*>& forms) {
  autofill_manager_.client()->PropagateAutofillPredictions(nullptr, forms);
};

void AutofillDriverIOS::SendAutofillTypePredictionsToRenderer(
    const std::vector<FormStructure*>& forms) {
  [bridge_ sendAutofillTypePredictionsToRenderer:
               FormStructure::GetFieldTypePredictions(forms)
                                         toFrame:web_frame_];
}

void AutofillDriverIOS::RendererShouldAcceptDataListSuggestion(
    const base::string16& value) {
}

void AutofillDriverIOS::DidInteractWithCreditCardForm() {
  if (!web::IsOriginSecure(web_state_->GetLastCommittedURL())) {
    autofill_manager_.client()->DidInteractWithNonsecureCreditCardInput();
  }
}

void AutofillDriverIOS::RendererShouldClearFilledSection() {}

void AutofillDriverIOS::RendererShouldClearPreviewedForm() {
}

void AutofillDriverIOS::RendererShouldFillFieldWithValue(
    const base::string16& value) {
}

void AutofillDriverIOS::RendererShouldPreviewFieldWithValue(
    const base::string16& value) {
}

void AutofillDriverIOS::PopupHidden() {
}

gfx::RectF AutofillDriverIOS::TransformBoundingBoxToViewportCoordinates(
    const gfx::RectF& bounding_box) {
  return bounding_box;
}

}  // namespace autofill
