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

  mojom::PixCodeDetectionResult IsPixCodeFound(
      FacilitatedPaymentsAgent* agent) {
    mojom::PixCodeDetectionResult actual =
        mojom::PixCodeDetectionResult::kPixCodeNotFound;
    static_cast<mojom::FacilitatedPaymentsAgent*>(agent)
        ->TriggerPixCodeDetection(base::BindOnce(
            [](mojom::PixCodeDetectionResult* output,
               mojom::PixCodeDetectionResult result) { *output = result; },
            /*output=*/&actual));
    return actual;
  }

 private:
  blink::AssociatedInterfaceRegistry associated_interfaces_;
};

TEST_F(FacilitatedPaymentsAgentTest, TriggerPixCodeDetection_NotFound) {
  EXPECT_EQ(mojom::PixCodeDetectionResult::kPixCodeNotFound,
            IsPixCodeFound(CreateAgentFor(R"(
   <body>
    <div>
      Hello world!
    </div>
  </form>
  )").get()));
}

TEST_F(FacilitatedPaymentsAgentTest, TriggerPixCodeDetection_FoundValid) {
  EXPECT_EQ(mojom::PixCodeDetectionResult::kValidPixCodeFound,
            IsPixCodeFound(CreateAgentFor(R"(
   <body>
    <div>
      00020126370014br.gov.bcb.pix2515www.example.com6304EA3F
    </div>
  </form>
  )").get()));
}

TEST_F(FacilitatedPaymentsAgentTest, TriggerPixCodeDetection_FoundInvalid) {
  EXPECT_EQ(mojom::PixCodeDetectionResult::kInvalidPixCodeFound,
            IsPixCodeFound(CreateAgentFor(R"(
   <body>
    <div>
      0014br.gov.bcb.pix
    </div>
  </form>
  )").get()));
}

TEST_F(FacilitatedPaymentsAgentTest, TriggerPixCodeDetection_FoundTwoInvalid) {
  EXPECT_EQ(mojom::PixCodeDetectionResult::kInvalidPixCodeFound,
            IsPixCodeFound(CreateAgentFor(R"(
   <body>
    <div>
      0014br.gov.bcb.pix
    </div>
    <div>
      0014br.gov.bcb.pix
    </div>
  </form>
  )").get()));
}

TEST_F(FacilitatedPaymentsAgentTest,
       TriggerPixCodeDetection_IgnoreFirstInvalid) {
  EXPECT_EQ(mojom::PixCodeDetectionResult::kValidPixCodeFound,
            IsPixCodeFound(CreateAgentFor(R"(
   <body>
    <div>
      0014br.gov.bcb.pix
    </div>
    <div>
      00020126370014br.gov.bcb.pix2515www.example.com6304EA3F
    </div>
  </form>
  )").get()));
}

TEST_F(FacilitatedPaymentsAgentTest,
       TriggerPixCodeDetection_IgnoreSecondInvalid) {
  EXPECT_EQ(mojom::PixCodeDetectionResult::kValidPixCodeFound,
            IsPixCodeFound(CreateAgentFor(R"(
   <body>
    <div>
      00020126370014br.gov.bcb.pix2515www.example.com6304EA3F
    </div>
    <div>
      0014br.gov.bcb.pix
    </div>
  </form>
  )").get()));
}

TEST_F(FacilitatedPaymentsAgentTest,
       TriggerPixCodeDetection_NotFoundWhenDeleting) {
  std::unique_ptr<FacilitatedPaymentsAgent> agent = CreateAgentFor(R"(
   <body>
    <div>
      00020126370014br.gov.bcb.pix2515www.example.com6304EA3F
    </div>
  </form>
  )");
  // Relesase the pointer because OnDestruct() calls DeleteSoon() on the PIX
  // agent.
  FacilitatedPaymentsAgent* unowned_agent = agent.release();

  static_cast<content::RenderFrameObserver*>(unowned_agent)->OnDestruct();

  EXPECT_EQ(mojom::PixCodeDetectionResult::kPixCodeDetectionNotRun,
            IsPixCodeFound(unowned_agent));
}

}  // namespace
}  // namespace payments::facilitated
