// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/content/renderer/facilitated_payments_agent.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace payments::facilitated {
namespace {

class FacilitatedPaymentsAgentTest : public content::RenderViewTest {
 public:
  std::unique_ptr<FacilitatedPaymentsAgent> CreateAgentFor(const char* html) {
    LoadHTML(html);
    return std::make_unique<FacilitatedPaymentsAgent>(GetMainRenderFrame(),
                                                      &associated_interfaces_);
  }

  void Expect(mojom::PixCodeDetectionResult expected_status_code,
              const std::string& expected_pix_code,
              FacilitatedPaymentsAgent* agent) {
    mojom::PixCodeDetectionResult actual_status_code =
        mojom::PixCodeDetectionResult::kPixCodeNotFound;
    std::string actual_pix_code;
    static_cast<mojom::FacilitatedPaymentsAgent*>(agent)
        ->TriggerPixCodeDetection(base::BindOnce(
            [](mojom::PixCodeDetectionResult* output_status_code,
               std::string* output_pix_code,
               mojom::PixCodeDetectionResult status_code,
               const std::string& pix_code) {
              *output_status_code = status_code;
              *output_pix_code = pix_code;
            },
            /*output_status_code=*/&actual_status_code,
            /*output_pix_code=*/&actual_pix_code));
    EXPECT_EQ(expected_status_code, actual_status_code);
    EXPECT_EQ(expected_pix_code, actual_pix_code);
  }

 private:
  blink::AssociatedInterfaceRegistry associated_interfaces_;
};

TEST_F(FacilitatedPaymentsAgentTest, TriggerPixCodeDetection_NotFound) {
  Expect(mojom::PixCodeDetectionResult::kPixCodeNotFound, std::string(),
         CreateAgentFor(R"(
   <body>
    <div>
      Hello world!
    </div>
  </body>
  )").get());
}

TEST_F(FacilitatedPaymentsAgentTest, TriggerPixCodeDetection_FoundValid) {
  Expect(mojom::PixCodeDetectionResult::kValidPixCodeFound,
         "00020126370014br.gov.bcb.pix2515www.example.com6304EA3F",
         CreateAgentFor(R"(
   <body>
    <div>
      00020126370014br.gov.bcb.pix2515www.example.com6304EA3F
    </div>
  </body>
  )").get());
}

TEST_F(FacilitatedPaymentsAgentTest,
       TriggerPixCodeDetection_FoundValidInInputElement) {
  Expect(mojom::PixCodeDetectionResult::kValidPixCodeFound,
         "00020126370014br.gov.bcb.pix2515www.example.com6304EA3F",
         CreateAgentFor(R"(
   <body>
    <input type="text" readonly=""
      value="00020126370014br.gov.bcb.pix2515www.example.com6304EA3F">
  </body>
  )").get());
}

TEST_F(FacilitatedPaymentsAgentTest,
       TriggerPixCodeDetection_NotFoundInEditableInput) {
  Expect(mojom::PixCodeDetectionResult::kPixCodeNotFound, std::string(),
         CreateAgentFor(R"(
   <body>
    <input type="text"
      value="00020126370014br.gov.bcb.pix2515www.example.com6304EA3F">
  </body>
  )").get());
}

TEST_F(FacilitatedPaymentsAgentTest,
       TriggerPixCodeDetection_NotFoundInNonTextInput) {
  Expect(mojom::PixCodeDetectionResult::kPixCodeNotFound, std::string(),
         CreateAgentFor(R"(
   <body>
    <input type="url" readonly=""
      value="00020126370014br.gov.bcb.pix2515www.example.com6304EA3F">
  </body>
  )").get());
}

TEST_F(FacilitatedPaymentsAgentTest,
       TriggerPixCodeDetection_FoundValid_IgnoreCase) {
  Expect(mojom::PixCodeDetectionResult::kValidPixCodeFound,
         "00020126370014BR.gov.bcb.PIX2515www.example.com6304EA3F",
         CreateAgentFor(R"(
   <body>
    <div>
      00020126370014BR.gov.bcb.PIX2515www.example.com6304EA3F
    </div>
  </body>
  )").get());
}

TEST_F(FacilitatedPaymentsAgentTest, TriggerPixCodeDetection_FoundInvalid) {
  Expect(mojom::PixCodeDetectionResult::kInvalidPixCodeFound, std::string(),
         CreateAgentFor(R"(
   <body>
    <div>
      0014br.gov.bcb.pix
    </div>
  </body>
  )").get());
}

TEST_F(FacilitatedPaymentsAgentTest, TriggerPixCodeDetection_FoundTwoInvalid) {
  Expect(mojom::PixCodeDetectionResult::kInvalidPixCodeFound, std::string(),
         CreateAgentFor(R"(
   <body>
    <div>
      0014br.gov.bcb.pix
    </div>
    <div>
      0014br.gov.bcb.pix
    </div>
  </body>
  )").get());
}

TEST_F(FacilitatedPaymentsAgentTest,
       TriggerPixCodeDetection_IgnoreFirstInvalid) {
  Expect(mojom::PixCodeDetectionResult::kValidPixCodeFound,
         "00020126370014br.gov.bcb.pix2515www.example.com6304EA3F",
         CreateAgentFor(R"(
   <body>
    <div>
      0014br.gov.bcb.pix
    </div>
    <div>
      00020126370014br.gov.bcb.pix2515www.example.com6304EA3F
    </div>
  </body>
  )").get());
}

TEST_F(FacilitatedPaymentsAgentTest,
       TriggerPixCodeDetection_IgnoreSecondInvalid) {
  Expect(mojom::PixCodeDetectionResult::kValidPixCodeFound,
         "00020126370014br.gov.bcb.pix2515www.example.com6304EA3F",
         CreateAgentFor(R"(
   <body>
    <div>
      00020126370014br.gov.bcb.pix2515www.example.com6304EA3F
    </div>
    <div>
      0014br.gov.bcb.pix
    </div>
  </body>
  )").get());
}

TEST_F(FacilitatedPaymentsAgentTest,
       TriggerPixCodeDetection_NotFoundWhenDeleting) {
  std::unique_ptr<FacilitatedPaymentsAgent> agent = CreateAgentFor(R"(
   <body>
    <div>
      00020126370014br.gov.bcb.pix2515www.example.com6304EA3F
    </div>
  </body>
  )");
  // Relesase the pointer because OnDestruct() calls DeleteSoon() on the PIX
  // agent.
  FacilitatedPaymentsAgent* unowned_agent = agent.release();

  static_cast<content::RenderFrameObserver*>(unowned_agent)->OnDestruct();

  Expect(mojom::PixCodeDetectionResult::kPixCodeDetectionNotRun, std::string(),
         unowned_agent);
}

}  // namespace
}  // namespace payments::facilitated
