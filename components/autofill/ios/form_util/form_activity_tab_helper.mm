// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/form_activity_tab_helper.h"

#import <Foundation/Foundation.h>

#import <optional>

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
#import "components/autofill/ios/common/field_data_manager_factory_ios.h"
#import "components/autofill/ios/form_util/form_activity_observer.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"

using autofill::FieldDataManager;
using autofill::FieldDataManagerFactoryIOS;
using autofill::FormData;
using base::SysUTF8ToNSString;

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

  web::WebFramesManager* frames_manager =
      GetWebFramesManagerForAutofill(web_state);
  web::WebFrame* sender_frame = frames_manager->GetFrameWithId(*frame_id);
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

  std::vector<FormData> forms;

  bool success = autofill::ExtractFormsData(
      base::SysUTF8ToNSString(form_data), true, base::UTF8ToUTF16(form_name),
      web_state->GetLastCommittedURL(), sender_frame->GetSecurityOrigin(),
      *fieldDataManager, sender_frame->GetFrameId(), &forms);

  if (!success || forms.size() != 1) {
    return;
  }

  FormData form = forms[0];

  for (auto& observer : observers_) {
    observer.DocumentSubmitted(web_state, sender_frame, form,
                               submitted_by_user);
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(FormActivityTabHelper)

}  // namespace autofill
