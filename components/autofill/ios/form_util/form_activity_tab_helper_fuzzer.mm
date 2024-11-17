// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/ios/form_util/form_activity_tab_helper.h"

#include "base/logging.h"
#import "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#import "base/test/ios/wait_util.h"
#include "ios/web/public/js_messaging/fuzzer_support/fuzzer_util.h"
#include "ios/web/public/js_messaging/fuzzer_support/js_message.pb.h"
#include "ios/web/public/js_messaging/script_message.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#include "ios/web/public/test/fuzzer_env_with_web_state.h"
#include "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/web_state.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

web::WebFrame* WaitForMainFrame(web::WebState* web_state) {
  __block web::WebFrame* main_frame = nullptr;
  DCHECK(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    main_frame = web_state->GetPageWorldWebFramesManager()->GetMainWebFrame();
    return main_frame != nullptr;
  }));
  return main_frame;
}

class Env : public web::FuzzerEnvWithWebState {
 public:
  Env() {
    // To create a main web frame.
    web::test::LoadHtml(@"<p>", web_state());
    web::WebFrame* main_frame = WaitForMainFrame(web_state());
    if (!main_frame) {
      LOG(ERROR) << "No main frame was created!";
    }
    main_frame_id_ = main_frame->GetFrameId();
    tab_helper_ =
        autofill::FormActivityTabHelper::GetOrCreateForWebState(web_state());
  }
  // The object will be deconstructed at deconstructing the WebState.
  raw_ptr<autofill::FormActivityTabHelper> tab_helper_;
  std::string main_frame_id_;
};

protobuf_mutator::protobuf::LogSilencer log_silencer;

}  // namespace

DEFINE_PROTO_FUZZER(const web::ScriptMessageProto& proto_js_message) {
  static Env env;

  std::unique_ptr<web::ScriptMessage> script_message =
      web::fuzzer::ProtoToScriptMessage(proto_js_message);

  // Insert the |frameID| of main frame in the |WebState| as initialized at
  // creating |Env|. This is because if the |frameID| in |ScriptMessage| is
  // invalid, the fuzzed API will return early and skip most of the interesting
  // logic.
  if (script_message->body() && script_message->body()->is_dict()) {
    // Insert the |frameID| at 98% probability. We still want to check how API
    // behaves at an invalid |frameID|.
    if (base::RandDouble() < 0.98) {
      script_message->body()->GetDict().Set("frameID", env.main_frame_id_);
    }
  }

  // The actual API being fuzzed.
  env.tab_helper_->OnFormMessageReceived(env.web_state(), *script_message);
}
