// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/renderer/facilitated_payments_agent.h"

#include <string>

#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/facilitated_payments/core/util/pix_code_validator.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace payments::facilitated {

FacilitatedPaymentsAgent::FacilitatedPaymentsAgent(
    content::RenderFrame* render_frame,
    blink::AssociatedInterfaceRegistry* registry)
    : content::RenderFrameObserver(render_frame) {
  registry->AddInterface<mojom::FacilitatedPaymentsAgent>(
      base::BindRepeating(&FacilitatedPaymentsAgent::BindPendingReceiver,
                          weak_ptr_factory_.GetWeakPtr()));
}

FacilitatedPaymentsAgent::~FacilitatedPaymentsAgent() = default;

void FacilitatedPaymentsAgent::TriggerPixCodeDetection(
    base::OnceCallback<void(mojom::PixCodeDetectionResult, const std::string&)>
        callback) {
  if (will_destruct_ || !render_frame() || !render_frame()->IsMainFrame() ||
      !render_frame()->GetWebFrame()) {
    std::move(callback).Run(
        mojom::PixCodeDetectionResult::kPixCodeDetectionNotRun, std::string());
    return;
  }

  mojom::PixCodeDetectionResult result =
      mojom::PixCodeDetectionResult::kPixCodeNotFound;
  std::string pix_code;
  constexpr char kPixCodeIdentifierLowercase[] = "0014br.gov.bcb.pix";
  // Discard the PIX code string.
  render_frame()->GetWebFrame()->GetDocument().FindTextInElementWith(
      blink::WebString(kPixCodeIdentifierLowercase),
      [&](const blink::WebString& potential_code) {
        std::string trimmed_result = base::UTF16ToUTF8(base::TrimWhitespace(
            potential_code.Utf16(), base::TrimPositions::TRIM_ALL));
        if (trimmed_result.empty()) {
          return false;
        }

        result = PixCodeValidator::IsValidPixCode(trimmed_result)
                     ? mojom::PixCodeDetectionResult::kValidPixCodeFound
                     : mojom::PixCodeDetectionResult::kInvalidPixCodeFound;

        if (result != mojom::PixCodeDetectionResult::kValidPixCodeFound) {
          return false;
        }

        pix_code = trimmed_result;
        return true;
      });

  std::move(callback).Run(result, pix_code);
}

void FacilitatedPaymentsAgent::OnDestruct() {
  will_destruct_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

void FacilitatedPaymentsAgent::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::FacilitatedPaymentsAgent>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

}  // namespace payments::facilitated
