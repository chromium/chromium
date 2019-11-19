// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/browser/autofill_driver_ios.h"

#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/ios/browser/autofill_driver_ios_bridge.h"
#include "components/autofill/ios/browser/autofill_driver_ios_webframe.h"
#import "ios/web/common/origin_util.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/web_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "ui/accessibility/ax_tree_id.h"
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
  // By the time this method is called, no web_frame is available. This method
  // only prepares the factory and the AutofillDriverIOS will be created in the
  // first call to FromWebStateAndWebFrame.
  AutofillDriverIOSWebFrameFactory::CreateForWebStateAndDelegate(
      web_state, client, bridge, app_locale, enable_download_manager);
}

// static
AutofillDriverIOS* AutofillDriverIOS::FromWebStateAndWebFrame(
    web::WebState* web_state,
    web::WebFrame* web_frame) {
    return AutofillDriverIOSWebFrameFactory::FromWebState(web_state)
        ->AutofillDriverIOSFromWebFrame(web_frame)
        ->driver();
}

AutofillDriverIOS::AutofillDriverIOS(
    web::WebState* web_state,
    web::WebFrame* web_frame,
    AutofillClient* client,
    id<AutofillDriverIOSBridge> bridge,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager)
    : web_state_(web_state),
      bridge_(bridge),
      autofill_manager_(this, client, app_locale, enable_download_manager),
      autofill_external_delegate_(&autofill_manager_, this) {
  web_frame_id_ = web::GetWebFrameId(web_frame);
  autofill_manager_.SetExternalDelegate(&autofill_external_delegate_);
}

AutofillDriverIOS::~AutofillDriverIOS() {}

bool AutofillDriverIOS::IsIncognito() const {
  return web_state_->GetBrowserState()->IsOffTheRecord();
}

bool AutofillDriverIOS::IsInMainFrame() const {
  web::WebFrame* web_frame = web::GetWebFrameWithId(web_state_, web_frame_id_);
  return web_frame ? web_frame->IsMainFrame() : true;
}

ui::AXTreeID AutofillDriverIOS::GetAxTreeId() const {
  NOTIMPLEMENTED() << "See https://crbug.com/985933";
  return ui::AXTreeIDUnknown();
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
  web::WebFrame* web_frame = web::GetWebFrameWithId(web_state_, web_frame_id_);
  if (!web_frame) {
    return;
  }
  [bridge_ fillFormData:data inFrame:web_frame];
}

void AutofillDriverIOS::PropagateAutofillPredictions(
    const std::vector<autofill::FormStructure*>& forms) {
  autofill_manager_.client()->PropagateAutofillPredictions(nullptr, forms);
}

void AutofillDriverIOS::SendAutofillTypePredictionsToRenderer(
    const std::vector<FormStructure*>& forms) {
  web::WebFrame* web_frame = web::GetWebFrameWithId(web_state_, web_frame_id_);
  if (!web_frame) {
    return;
  }
  [bridge_ fillFormDataPredictions:FormStructure::GetFieldTypePredictions(forms)
                           inFrame:web_frame];
}

void AutofillDriverIOS::RendererShouldAcceptDataListSuggestion(
    const base::string16& value) {
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

void AutofillDriverIOS::RendererShouldSetSuggestionAvailability(
    const mojom::AutofillState state) {}

void AutofillDriverIOS::PopupHidden() {
}

gfx::RectF AutofillDriverIOS::TransformBoundingBoxToViewportCoordinates(
    const gfx::RectF& bounding_box) {
  return bounding_box;
}

net::NetworkIsolationKey AutofillDriverIOS::NetworkIsolationKey() {
  std::string main_web_frame_id = web::GetMainWebFrameId(web_state_);
  web::WebFrame* main_web_frame =
      web::GetWebFrameWithId(web_state_, main_web_frame_id);
  if (!main_web_frame)
    return net::NetworkIsolationKey();

  web::WebFrame* web_frame = web::GetWebFrameWithId(web_state_, web_frame_id_);
  if (!web_frame)
    return net::NetworkIsolationKey();

  return net::NetworkIsolationKey(
      url::Origin::Create(main_web_frame->GetSecurityOrigin()),
      url::Origin::Create(web_frame->GetSecurityOrigin()));
}

}  // namespace autofill
