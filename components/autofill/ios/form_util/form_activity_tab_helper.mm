// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/form_activity_tab_helper.h"

#import <Foundation/Foundation.h>

#import <optional>
#import <variant>

#import "base/debug/crash_logging.h"
#import "base/debug/dump_without_crashing.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/not_fatal_until.h"
#import "base/notreached.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/values.h"
#import "components/autofill/core/common/autofill_data_validation.h"
#import "components/autofill/core/common/autofill_util.h"
#import "components/autofill/core/common/field_data_manager.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/common/constants.h"
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

// Enumeration describing the possible outcomes of handling a form submission
// message.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(FormSubmissionOutcomeIOS)
enum class FormSubmissionOutcome {
  // Form submission message was valid. Observers were notified about the form
  // submission event.
  kHandled = 0,
  // Form submission message did not have a body or wasn't a dict.
  kInvalidMessageBody = 1,
  // Form submission message did not have a frame ID.
  kNoFrameID = 2,
  // There is no existing WebFrame for the submitted form.
  kNoFrame = 3,
  // The submission message indicates it originated from the main
  // frame while the matching WebFrame is not or viceversa.
  kIsMainFrameDiscrepancy = 4,
  // Form submission message is missing the href field.
  kMissingHref = 5,
  // The payload was missing the `formData` field or it wasn't a base::Dict.
  kMissingFormData = 6,
  // The formData was invalid. Inspect
  // Autofill.iOS.FormSubmission.Outcome.InvalidFormReason to investigate the
  // cause.
  kFormExtractionFailure = 7,
  // There was an error while handling the form submission event in the
  // renderer.
  kRendererError = 8,
  // There was an error while handling the form submission event in the renderer
  // but the error couldn't be parsed.
  kUnparsedRendererError = 9,
  kMaxValue = kUnparsedRendererError,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:FormSubmissionOutcomeIOS)

// LINT.IfChange(autofill_count_form_submission_in_renderer)
// Source that triggered the form submission report.
enum class FormSubmissionReportSource {
  // Report was sent immediately because quota was available.
  kInstant = 0,
  // Report was sent from the scheduled task.
  kScheduledTask = 1,
  // Report was sent from unloading the page content.
  kUnloadPage = 2,
  kMaxValue = kUnloadPage,
};
// LINT.ThenChange(//components/autofill/ios/form_util/resources/form.ts:autofill_count_form_submission_in_renderer)

// Returns the suffix describing the `source`.
std::string FormSubmissionReportSourceToSuffix(
    FormSubmissionReportSource source) {
  switch (source) {
    case FormSubmissionReportSource::kInstant:
      return "FromInstant";
    case FormSubmissionReportSource::kUnloadPage:
      return "FromUnloadPage";
    case FormSubmissionReportSource::kScheduledTask:
      return "FromScheduledTask";
  }
}

// Returns the submission detection metric name combined with the `suffix`.
std::string GetFormSubmissionDetectionMetricName(std::string_view suffix) {
  return base::StrCat(
      {"Autofill.iOS.FormActivity.SubmissionDetectedBeforeProcessing.PerType",
       ".", suffix});
}

void RecordFormActivityMetrics(const base::Value::Dict& message_body) {
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

// Record the form submission count metrics provided in the `message_body`.
void RecordFormSubmissionCountMetrics(const base::Value::Dict& message_body) {
  if (!base::FeatureList::IsEnabled(kAutofillCountFormSubmissionInRenderer)) {
    return;
  }

  std::optional<double> html_event_count = message_body.FindDouble("htmlEvent");
  std::optional<double> programmatic_count =
      message_body.FindDouble("programmatic");

  auto SourceEnumFromNumber =
      [](std::optional<double> s) -> std::optional<FormSubmissionReportSource> {
    if (!s) {
      return std::nullopt;
    }
    switch (static_cast<int>(*s)) {
      case 0:
        return FormSubmissionReportSource::kInstant;
      case 1:
        return FormSubmissionReportSource::kScheduledTask;
      case 2:
        return FormSubmissionReportSource::kUnloadPage;
    }
    return std::nullopt;
  };
  std::optional<FormSubmissionReportSource> source =
      SourceEnumFromNumber(message_body.FindDouble("source"));

  if (!html_event_count || !programmatic_count || !source) {
    base::UmaHistogramEnumeration(
        GetFormSubmissionDetectionMetricName("FromAll"),
        /*sample=*/CountedSubmissionType::kCantParse);
  }

  if (!source) {
    SCOPED_CRASH_KEY_NUMBER("FormSubmissionReport", "invalid-source",
                            static_cast<int>(*source));
    NOTREACHED(base::NotFatalUntil::M141);
    return;
  }

  // Record one histogram for each count and type as we want to see
  // the total number of occurrences for each type. This is a way of dealing
  // with the fact that the detected form submissions are reported in batches
  // (i.e. are aggregated) for performance reasons, so we need to disaggregate
  // the events before reporting them.
  auto RecordForEachType = [source = *source](int count,
                                              CountedSubmissionType type) {
    for (int i = 0; i < count; ++i) {
      std::string suffix = FormSubmissionReportSourceToSuffix(source);
      base::UmaHistogramEnumeration(
          GetFormSubmissionDetectionMetricName(suffix),
          /*sample=*/type);
      base::UmaHistogramEnumeration(
          GetFormSubmissionDetectionMetricName("FromAll"),
          /*sample=*/type);
    }
  };
  RecordForEachType(static_cast<int>(*html_event_count),
                    CountedSubmissionType::kHtmlEvent);
  RecordForEachType(static_cast<int>(*programmatic_count),
                    CountedSubmissionType::kProgrammatic);

  base::UmaHistogramCounts100(
      "Autofill.iOS.FormActivity.SubmissionDetectedBeforeProcessing.BatchSize",
      static_cast<int>(*html_event_count + *programmatic_count));
}

// Logs the outcome of form submissions to metrics.
void RecordFormSubmissionOutcome(FormSubmissionOutcome outcome) {
  base::UmaHistogramEnumeration(autofill::kFormSubmissionOutcomeHistogram,
                                outcome);
}

// Logs form submission failures due to invalid form data.
void RecordFormExtractionFailure(autofill::ExtractFormDataFailure failure) {
  base::UmaHistogramEnumeration(autofill::kInvalidSubmittedFormReasonHistogram,
                                failure);
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
  if (std::optional<base::UnguessableToken> remote =
          autofill::DeserializeJavaScriptFrameId(remote_frame_token)) {
    auto* registrar =
        autofill::ChildFrameRegistrar::GetOrCreateForWebState(web_state);
    return registrar->LookupChildFrame(autofill::RemoteFrameToken(*remote));
  }

  SCOPED_CRASH_KEY_STRING32(/*category=*/"FormSubmission",
                            /*name=*/"remote_frame_token",
                            /*value=*/remote_frame_token);
  base::debug::DumpWithoutCrashing();

  return std::nullopt;
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

void HandleSubmissionError(const base::Value::Dict& message) {
  const std::string* error_stack = message.FindString("errorStack");
  const std::string* error_message = message.FindString("errorMessage");
  std::optional<bool> is_programmatic =
      message.FindBool("programmaticSubmission");

  if (!error_stack || !error_message || !is_programmatic) {
    RecordFormSubmissionOutcome(FormSubmissionOutcome::kUnparsedRendererError);
    return;
  }

  SCOPED_CRASH_KEY_STRING256("FormSubmissionError", "msg", *error_message);
  SCOPED_CRASH_KEY_STRING1024("FormSubmissionError", "stack", *error_stack);

  base::debug::DumpWithoutCrashing();

  RecordFormSubmissionOutcome(FormSubmissionOutcome::kRendererError);
}

void FormActivityTabHelper::OnFormMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  if (!message.body() || !message.body()->is_dict()) {
    // Ignore invalid message.
    return;
  }

  const auto& message_body = message.body()->GetDict();

  RecordFormActivityMetrics(message_body);

  const std::string* command = message_body.FindString("command");
  if (!command) {
    DLOG(WARNING) << "JS message parameter not found: command";
  } else if (*command == "form.submit") {
    FormSubmissionHandler(web_state, message);
  } else if (*command == "form.activity") {
    HandleFormActivity(web_state, message);
  } else if (*command == "form.removal") {
    HandleFormRemoval(web_state, message);
  } else if (*command == "form.submit.count") {
    RecordFormSubmissionCountMetrics(message_body);
  } else if (*command == "form.submit.error") {
    HandleSubmissionError(message_body);
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
    RecordFormSubmissionOutcome(FormSubmissionOutcome::kInvalidMessageBody);
    return;
  }

  const base::Value::Dict& message_body = message.body()->GetDict();
  const std::string* frame_id = message_body.FindString("frameID");
  if (!frame_id) {
    RecordFormSubmissionOutcome(FormSubmissionOutcome::kNoFrameID);
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
      RecordFormSubmissionOutcome(FormSubmissionOutcome::kNoFrame);
      return;
    }
  } else {
    // Handle isolated world messages without any special consideration.
    web::WebFramesManager* frames_manager =
        GetWebFramesManagerForAutofill(web_state);
    sender_frame = frames_manager->GetFrameWithId(*frame_id);
  }

  if (!sender_frame) {
    RecordFormSubmissionOutcome(FormSubmissionOutcome::kNoFrame);
    return;
  }

  if (sender_frame->IsMainFrame() != message.is_main_frame()) {
    RecordFormSubmissionOutcome(FormSubmissionOutcome::kIsMainFrameDiscrepancy);
    return;
  }

  if (!message_body.FindString("href")) {
    DLOG(WARNING) << "JS message parameter not found: href";
    RecordFormSubmissionOutcome(FormSubmissionOutcome::kMissingHref);
    return;
  }
  const std::string* maybe_form_name = message_body.FindString("formName");

  // We decide the form is user-submitted if the user has interacted with
  // the main page (using logic from the popup blocker), or if the keyboard
  // is visible.
  BOOL submitted_by_user = message.is_user_interacting() ||
                           web_state->GetWebViewProxy().keyboardVisible;

  std::string form_name;
  if (maybe_form_name) {
    form_name = *maybe_form_name;
  }

  FieldDataManager* fieldDataManager =
      FieldDataManagerFactoryIOS::FromWebFrame(sender_frame);

  const base::Value::Dict* form_data = message_body.FindDict("formData");
  if (!form_data) {
    RecordFormSubmissionOutcome(FormSubmissionOutcome::kMissingFormData);
    return;
  }

  // We need to pass `frame_id` when extracting the form even if the frame is
  // from the page world. `ExtractFormData` checks that the frame id matches
  // the id of the frame that contains the forms. For page world forms, we set
  // FormData::host_frame with the corresponding isolated world frame in
  // `local_frame_token`.
  std::variant<FormData, ExtractFormDataFailure> form_or_failure =
      autofill::ExtractFormDataOrFailure(
          *form_data, true, base::UTF8ToUTF16(form_name),
          web_state->GetLastCommittedURL(), sender_frame->GetSecurityOrigin(),
          *fieldDataManager, *frame_id, local_frame_token);

  if (std::holds_alternative<ExtractFormDataFailure>(form_or_failure)) {
    RecordFormSubmissionOutcome(FormSubmissionOutcome::kFormExtractionFailure);
    RecordFormExtractionFailure(
        std::get<ExtractFormDataFailure>(form_or_failure));
    return;
  }

  FormData form = std::get<FormData>(form_or_failure);

  if (std::optional<bool> programmatic_submission =
          message_body.FindBool("programmaticSubmission")) {
    base::UmaHistogramBoolean(kProgrammaticFormSubmissionHistogram,
                              *programmatic_submission);
  }

  // A form is considered "perfectly filled" if none of its fields were edited
  // by the user, unless that field was autofilled in the first place.
  const bool perfect_filling = IsFormPerfectlyFilled(form);

  for (auto& observer : observers_) {
    observer.DocumentSubmitted(web_state, sender_frame, form, submitted_by_user,
                               perfect_filling);
  }

  RecordFormSubmissionOutcome(FormSubmissionOutcome::kHandled);
}

}  // namespace autofill
