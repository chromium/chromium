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

  bool IsPixCodeFound(FacilitatedPaymentsAgent* agent) {
    bool actual = false;
    static_cast<mojom::FacilitatedPaymentsAgent*>(agent)
        ->TriggerPixCodeDetection(base::BindOnce(
            [](bool* output, bool found_pix_code) { *output = found_pix_code; },
            /*output=*/&actual));
    return actual;
  }

 private:
  blink::AssociatedInterfaceRegistry associated_interfaces_;
};

TEST_F(FacilitatedPaymentsAgentTest, TriggerPixCodeDetection_NotFound) {
  EXPECT_FALSE(IsPixCodeFound(CreateAgentFor(R"(
   <body>
    <div>
      Hello world!
    </div>
  </form>
  )").get()));
}

TEST_F(FacilitatedPaymentsAgentTest, TriggerPixCodeDetection_Found) {
  EXPECT_TRUE(IsPixCodeFound(CreateAgentFor(R"(
   <body>
    <div>
      ABC0014br.gov.bcb.pixXYZ
    </div>
  </form>
  )").get()));
}

TEST_F(FacilitatedPaymentsAgentTest,
       TriggerPixCodeDetection_NotFoundWhenDeleting) {
  std::unique_ptr<FacilitatedPaymentsAgent> agent = CreateAgentFor(R"(
   <body>
    <div>
      ABC0014br.gov.bcb.pixXYZ
    </div>
  </form>
  )");
  // Relesase the pointer because OnDestruct() calls DeleteSoon() on the PIX
  // agent.
  FacilitatedPaymentsAgent* unowned_agent = agent.release();

  static_cast<content::RenderFrameObserver*>(unowned_agent)->OnDestruct();

  EXPECT_FALSE(IsPixCodeFound(unowned_agent));
}

}  // namespace
}  // namespace payments::facilitated
