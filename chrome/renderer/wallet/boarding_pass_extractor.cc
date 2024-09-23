// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/renderer/wallet/boarding_pass_extractor.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "v8/include/v8-isolate.h"

namespace wallet {

namespace {
const std::string GetDefaultExtractionScript() {
  std::string script = R"###(
    // Check if the str contains 'boarding pass'.
    function containBoardingPass(str) {
        return str.toLowerCase().replace(/\s+/g, '').includes('boardingpass');
    }

    // Scale up the base64 encoded image to be detectable by BarcodeDetector.
    function scaleUpIfNeeded(img) {
        if (img.src.startsWith('data:image/')
            && (img.width < 500 || image.height < 500)) {
            let canvas = new OffscreenCanvas(500, 500);
            let ctx = canvas.getContext('2d');
            ctx.ImageSmoothingEnabled = false;
            ctx.drawImage(img, 0, 0, 500, 500);
            return canvas;
        }
        return img;
    }

    // Check if the rawValue is a valid BCBP string.
    function isValidBoardingPass(rawValue) {
        return rawValue.length > 60 && /^M[1-9]/.test(rawValue);
    }

    // Detect and decode boarding passes on current web page.
    async function detectBoardingPass() {
        var results = [];
        // Check if BarcodeDetector is supported by current browser.
        if (!'BarcodeDetector' in window) {
            return [];
        }

        // Check if current URL or page title contains 'boarding pass'.
        if (!containBoardingPass(window.location.href)
            && !containBoardingPass(document.title)) {
            return [];
        }

        var detector;
        try {
            detector = new BarcodeDetector();
        } catch (error) {
            return [];
        }
        for (let i = 0; i < document.images.length; ++i) {
            try {
                const barcodes = await detector.detect(
                    scaleUpIfNeeded(document.images[i]));
                for (const barcode of barcodes) {
                    // Check if it's a valid boarding pass
                    if (barcode.rawValue
                        && isValidBoardingPass(barcode.rawValue)) {
                        results.push(barcode.rawValue);
                    }
                }
            } catch (error) {
            }
        }
        return results;
    }

    detected_results_promise = detectBoardingPass();
  )###";
  return script;
}

std::vector<std::string> ConvertResultsToStrings(base::Value& value) {
  std::vector<std::string> results;

  if (!value.is_list()) {
    return results;
  }

  for (const auto& item : value.GetList()) {
    if (!item.is_string()) {
      continue;
    }
    results.push_back(item.GetString());
  }
  return results;
}
}  // namespace

BoardingPassExtractor::BoardingPassExtractor(
    content::RenderFrame* render_frame,
    service_manager::BinderRegistry* registry)
    : content::RenderFrameObserver(render_frame) {
  // Being a RenderFrameObserver, this object is scoped to the RenderFrame.
  // Unretained is safe here because `registry` is also scoped to the
  // RenderFrame.
  registry->AddInterface(base::BindRepeating(
      &BoardingPassExtractor::BindReceiver, base::Unretained(this)));

  mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;
  content::RenderThread::Get()->BindHostReceiver(
      factory.BindNewPipeAndPassReceiver());
  ukm_recorder_ = ukm::MojoUkmRecorder::Create(*factory);
}

BoardingPassExtractor::~BoardingPassExtractor() = default;

void BoardingPassExtractor::BindReceiver(
    mojo::PendingReceiver<mojom::BoardingPassExtractor> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void BoardingPassExtractor::OnDestruct() {
  delete this;
}

void BoardingPassExtractor::ExtractBoardingPass(
    ExtractBoardingPassCallback callback) {
  ExtractBoardingPassWithScript(GetDefaultExtractionScript(),
                                std::move(callback));
}

void BoardingPassExtractor::ExtractBoardingPassWithScript(
    const std::string& script,
    ExtractBoardingPassCallback callback) {
  blink::WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  v8::HandleScope handle_scope(main_frame->GetAgentGroupScheduler()->Isolate());
  blink::WebScriptSource source =
      blink::WebScriptSource(blink::WebString::FromUTF8(script));

  main_frame->RequestExecuteScript(
      ISOLATED_WORLD_ID_CHROME_INTERNAL, base::span_from_ref(source),
      blink::mojom::UserActivationOption::kDoNotActivate,
      blink::mojom::EvaluationTiming::kAsynchronous,
      blink::mojom::LoadEventBlockingOption::kDoNotBlock,
      base::BindOnce(&BoardingPassExtractor::OnBoardingPassExtracted,
                     base::Unretained(this), std::move(callback)),
      blink::BackForwardCacheAware::kAllow,
      blink::mojom::WantResultOption::kWantResult,
      blink::mojom::PromiseResultOption::kAwait);
}

void BoardingPassExtractor::OnBoardingPassExtracted(
    ExtractBoardingPassCallback callback,
    std::optional<base::Value> results,
    base::TimeTicks start_time) {
  std::vector<std::string> boarding_passes;
  if (results.has_value()) {
    boarding_passes = ConvertResultsToStrings(*results);
  }

  ukm::builders::Wallet_BoardingPassDetect(
      render_frame()->GetWebFrame()->GetDocument().GetUkmSourceId())
      .SetDetected(!boarding_passes.empty())
      .Record(ukm_recorder_.get());

  UMA_HISTOGRAM_BOOLEAN("Android.Wallet.BoardingPass.Detected",
                        !boarding_passes.empty());

  std::move(callback).Run(std::move(boarding_passes));
}

}  // namespace wallet
