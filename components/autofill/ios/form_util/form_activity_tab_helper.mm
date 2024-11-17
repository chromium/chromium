// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/form_activity_tab_helper.h"

#import <Foundation/Foundation.h>

#import <optional>

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/values.h"
#import "components/autofill/core/common/field_data_manager.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/common/features.h"
#import "components/autofill/ios/common/field_data_manager_factory_ios.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "components/autofill/ios/form_util/form_activity_observer.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"

using autofill::FieldDataManager;
using autofill::FieldDataManagerFactoryIOS;
using autofill::FormData;
using autofill::LocalFrameToken;
using base::SysUTF8ToNSString;
using web::ContentWorld;
using web::WebFrame;

namespace {

void RecordMetrics(const base::Value::Dict& message_body) {
  const base::Value::Dict* metadata = message_body.FindDict("metadata");

  if (!metadata) {
    // Don't record metrics if no metadata because all the data for calculating
    // the metrics is there.
    return;
  }

  // Extract the essential data for metrics.
  std::optional<double> drop_count = metadata->FindDouble("dropCount");
  std::optional<double> batch_size = metadata->FindDouble("size");

  // Don't record metrics if there is missing data.
  if (!drop_count || !batch_size) {
    return;
  }

  // Record the number of dropped form activities.
  base::UmaHistogramCounts100("Autofill.iOS.FormActivity.DropCount",
                              *drop_count);

  // Record the number of messages sent in the batch.
  base::UmaHistogramCounts100("Autofill.iOS.FormActivity.SendCount",
                              *batch_size);

  // Record the ratio of sent messages on total eligible messages.
  if (const int denominator = *batch_size + *drop_count; denominator > 0) {
    const int percentage = (100 * (*batch_size)) / denominator;
    base::UmaHistogramPercentage("Autofill.iOS.FormActivity.SendRatio",
                                 percentage);
  }
}

// Finds the frame with `frame_id` in the given content world.
web::WebFrame* GetFrameInContentWorld(std::string frame_id,
                                      ContentWorld content_world,
                                      web::WebState* web_state) {
  auto* frames_manager = web_state->GetWebFramesManager(content_world);
  return frames_manager->GetFrameWithId(frame_id);
}

// Finds the local frame token associated to
// `remote_frame_token` in autofill::ChildFrameRegistrar.
std::optional<LocalFrameToken> LookupLocalFrame(std::string remote_frame_token,
                                                web::WebState* web_state) {
  std::optional<base::UnguessableToken> remote =
      autofill::DeserializeJavaScriptFrameId(remote_frame_token);

  auto* registrar =
      autofill::ChildFrameRegistrar::GetOrCreateForWebState(web_state);
  return registrar->LookupChildFrame(autofill::RemoteFrameToken(*remote));
}

// Finds the WebFrame in the isolated content world corresponding to a page
// content world WebFrame with id `page_world_frame_id`. The isolated world
// frame is retrieved using `remote_frame_token`, which should be associated to
// the frame's id in autofill::ChildFrameRegistrar.
//
// Returns std::nullopt if the
// isolated frame cannot be found or it has a different origin than its
// corresponding page world frame. Otherwise it returns the isolated world
// WebFrame along with its LocalFrameToken.
std::optional<std::pair<WebFrame*, LocalFrameToken>> GetIsolatedFrame(
    const std::string& page_world_frame_id,
    const std::string& remote_frame_token,
    web::WebState* web_state) {
  if (!base::FeatureList::IsEnabled(kAutofillIsolatedWorldForJavascriptIos)) {
    return std::nullopt;
  }

  std::optional<LocalFrameToken> local_frame_token =
      LookupLocalFrame(remote_frame_token, web_state);

  if (!local_frame_token) {
    return std::nullopt;
  }

  // Verify that the sender frame in the page world has the same origin as the
  // isolated one. Avoid cross origin talk.
  web::WebFrame* isolated_world_frame = GetFrameInContentWorld(
      local_frame_token->ToString(), ContentWorld::kIsolatedWorld, web_state);
  web::WebFrame* page_world_frame = GetFrameInContentWorld(
      page_world_frame_id, ContentWorld::kPageContentWorld, web_state);

  if (!isolated_world_frame || !page_world_frame) {
    return std::nullopt;
  }

  if (isolated_world_frame->GetSecurityOrigin() !=
      page_world_frame->GetSecurityOrigin()) {
    return std::nullopt;
  }

  return std::make_pair(isolated_world_frame, *local_frame_token);
}

}  // namespace

namespace autofill {

// static
FormActivityTabHelper* FormActivityTabHelper::GetOrCreateForWebState(
    web::WebState* web_state) {
  FormActivityTabHelper* helper = FromWebState(web_state);
  if (!helper) {
    CreateForWebState(web_state);
    helper = FromWebState(web_state);
    DCHECK(helper);
  }
  return helper;
}

FormActivityTabHelper::FormActivityTabHelper(web::WebState* web_state) {}
FormActivityTabHelper::~FormActivityTabHelper() = default;

void FormActivityTabHelper::AddObserver(FormActivityObserver* observer) {
  observers_.AddObserver(observer);
}

void FormActivityTabHelper::RemoveObserver(FormActivityObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FormActivityTabHelper::OnFormMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  if (!message.body() || !message.body()->is_dict()) {
    // Ignore invalid message.
    return;
  }

  RecordMetrics(message.body()->GetDict());

  const std::string* command = message.body()->GetDict().FindString("command");
  if (!command) {
    DLOG(WARNING) << "JS message parameter not found: command";
  } else if (*command == "form.submit") {
    FormSubmissionHandler(web_state, message);
  } else if (*command == "form.activity") {
    HandleFormActivity(web_state, message);
  } else if (*command == "form.removal") {
    HandleFormRemoval(web_state, message);
  }
}

void FormActivityTabHelper::HandleFormActivity(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  FormActivityParams params;
  if (!FormActivityParams::FromMessage(message, &params)) {
    return;
  }

  web::WebFramesManager* frames_manager =
      GetWebFramesManagerForAutofill(web_state);
  web::WebFrame* sender_frame = frames_manager->GetFrameWithId(params.frame_id);
  if (!sender_frame) {
    return;
  }

  for (auto& observer : observers_)
    observer.FormActivityRegistered(web_state, sender_frame, params);
}

void FormActivityTabHelper::HandleFormRemoval(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  FormRemovalParams params;
  if (!FormRemovalParams::FromMessage(message, &params)) {
    return;
  }

  web::WebFramesManager* frames_manager =
      GetWebFramesManagerForAutofill(web_state);
  web::WebFrame* sender_frame = frames_manager->GetFrameWithId(params.frame_id);
  if (!sender_frame) {
    return;
  }

  for (auto& observer : observers_)
    observer.FormRemoved(web_state, sender_frame, params);
}

void FormActivityTabHelper::FormSubmissionHandler(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  if (!message.body() || !message.body()->is_dict()) {
    // Ignore invalid message.
    return;
  }

  const base::Value::Dict& message_body = message.body()->GetDict();
  const std::string* frame_id = message_body.FindString("frameID");
  if (!frame_id) {
    return;
  }

  web::WebFrame* sender_frame = nullptr;

  const std::string* remote_frame_token =
      message_body.FindString("remoteFrameToken");
  LocalFrameToken local_frame_token;

  // The submit message came from a page world frame. Autofill works on the
  // isolated world space so map the page frame to the corresponding isolated
  // world one.
  if (remote_frame_token) {
    if (std::optional<std::pair<WebFrame*, LocalFrameToken>>
            isolated_frame_with_token =
                GetIsolatedFrame(*frame_id, *remote_frame_token, web_state)) {
      sender_frame = isolated_frame_with_token->first;
      local_frame_token = isolated_frame_with_token->second;
    } else {
      return;
    }
  } else {
    // Handle isolated world messages without any special consideration.
    web::WebFramesManager* frames_manager =
        GetWebFramesManagerForAutofill(web_state);
    sender_frame = frames_manager->GetFrameWithId(*frame_id);
  }

  if (!sender_frame) {
    return;
  }

  if (sender_frame->IsMainFrame() != message.is_main_frame()) {
    return;
  }

  if (!message_body.FindString("href")) {
    DLOG(WARNING) << "JS message parameter not found: href";
    return;
  }
  const std::string* maybe_form_name = message_body.FindString("formName");
  const std::string* maybe_form_data = message_body.FindString("formData");

  // We decide the form is user-submitted if the user has interacted with
  // the main page (using logic from the popup blocker), or if the keyboard
  // is visible.
  BOOL submitted_by_user = message.is_user_interacting() ||
                           web_state->GetWebViewProxy().keyboardVisible;

  std::string form_name;
  if (maybe_form_name) {
    form_name = *maybe_form_name;
  }
  std::string form_data;
  if (maybe_form_data) {
    form_data = *maybe_form_data;
  }

  FieldDataManager* fieldDataManager =
      FieldDataManagerFactoryIOS::FromWebFrame(sender_frame);
  // We need to pass `frame_id` when extracting the form even if the frame is
  // from the page world. `ExtractFormsData` checks that the frame id matches
  // the id of the frame that contains the forms. For page world forms, we set
  // FormData::host_frame with the corresponding isolated world frame in
  // `local_frame_token`.
  std::optional<std::vector<FormData>> forms = autofill::ExtractFormsData(
      base::SysUTF8ToNSString(form_data), true, base::UTF8ToUTF16(form_name),
      web_state->GetLastCommittedURL(), sender_frame->GetSecurityOrigin(),
      *fieldDataManager, *frame_id, local_frame_token);
  if (!forms || forms->size() != 1) {
    return;
  }

  if (std::optional<bool> programmatic_submission =
          message_body.FindBool("programmaticSubmission")) {
    base::UmaHistogramBoolean(kProgrammaticFormSubmissionHistogram,
                              *programmatic_submission);
  }
  FormData form = forms.value()[0];

  for (auto& observer : observers_) {
    observer.DocumentSubmitted(web_state, sender_frame, form,
                               submitted_by_user);
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(FormActivityTabHelper)

}  // namespace autofill
