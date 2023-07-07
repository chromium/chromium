// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <google/protobuf/descriptor.h>
#include <stddef.h>
#include <stdint.h>
#include <cstdint>
#include <vector>
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/in_process_fuzzer.h"
#include "chrome/test/fuzzing/kombucha_in_process_fuzzer.pb.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interactive_test.h"

// The following includes are used to enable ui_controls only.
#include "ui/base/test/ui_controls.h"
#if BUILDFLAG(IS_OZONE)
#include "ui/views/test/test_desktop_screen_ozone.h"
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/test/ui_controls_ash.h"
#elif BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "ui/aura/test/ui_controls_aurawin.h"
#endif
#if defined(USE_AURA) && BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/common/platform_window_defaults.h"
#endif  // defined(USE_AURA) && BUILDFLAG(IS_OZONE)

#define DEFINE_BINARY_PROTO_IN_PROCESS_FUZZER(arg) \
  DEFINE_PROTO_FUZZER_IN_PROCESS_IMPL(true, arg)

#define DEFINE_PROTO_FUZZER_IN_PROCESS_IMPL(use_binary, arg)      \
  static void TestOneProtoInput(arg);                             \
  using FuzzerProtoType =                                         \
      protobuf_mutator::libfuzzer::macro_internal::GetFirstParam< \
          decltype(&TestOneProtoInput)>::type;                    \
  DEFINE_CUSTOM_PROTO_MUTATOR_IMPL(use_binary, FuzzerProtoType)   \
  DEFINE_CUSTOM_PROTO_CROSSOVER_IMPL(use_binary, FuzzerProtoType) \
  DEFINE_POST_PROCESS_PROTO_MUTATION_IMPL(FuzzerProtoType)

class KombuchaInProcessFuzzer
    : virtual public InteractiveBrowserTestT<InProcessFuzzer> {
 public:
  ~KombuchaInProcessFuzzer() override = default;
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kTabGroupsSave, features::kExtensionsMenuInAppMenu}, {});

    // Mouse movements require enabling ui_controls manually for tests
    // that live outside the ui_interaction_test directory.
    // The following is copied from
    // chrome/test/base/interactive_ui_tests_main.cc
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ash::test::EnableUIControlsAsh();
#elif BUILDFLAG(IS_WIN)
    com_initializer_ = std::make_unique<base::win::ScopedCOMInitializer>();
    aura::test::EnableUIControlsAuraWin();
#elif BUILDFLAG(IS_OZONE)
    // Notifies the platform that test config is needed. For Wayland, for
    // example, makes its possible to use emulated input.
    ui::test::EnableTestConfigForPlatformWindows();
    ui::OzonePlatform::InitParams params;
    params.single_process = true;
    ui::OzonePlatform::InitializeForUI(params);
    ui_controls::EnableUIControls();
#else
    ui_controls::EnableUIControls();
#endif

    InteractiveBrowserTestT::SetUp();
  }
  void SetUpOnMainThread() override;

#if BUILDFLAG(IS_WIN)
  void TearDown() override {
    InteractiveBrowserTestT::TearDown();
    com_initializer_.reset();
  }

#endif
  using FuzzCase = test::fuzzing::ui_fuzzing::FuzzCase;
  int Fuzz(const uint8_t* data, size_t size) override;
  static std::unique_ptr<net::test_server::HttpResponse> HandleHTTPRequest(
      base::WeakPtr<KombuchaInProcessFuzzer> fuzzer_weak,
      const net::test_server::HttpRequest& request);

  FuzzCase current_fuzz_case_;

  // Step could be a StepBuilder or a Multistep
  template <typename T>
  auto CheckStep(ui::ElementIdentifier target, T step, std::string step_name) {
    return Steps(IfElement(
        target,
        [](const ui::TrackedElement* element) { return element != nullptr; },
        Steps(std::move(step), Log("[KOMB] Added ", step_name, " with target ",
                                   target.GetName())),
        Steps(Log("[KOMB] Failed to add step: ", step_name, ". ",
                  target.GetName(), " was not visible"))));
  }

  // Used primarily for DragFromTo which requires two ElementIdentifiers
  //  Check for dest first. If there is no source, we convert to DragMouseTo
  template <typename T>
  auto CheckStep(ui::ElementIdentifier source,
                 ui::ElementIdentifier dest,
                 T step,
                 std::string step_name) {
    return Steps(IfElement(
        dest,
        [](const ui::TrackedElement* element) { return element != nullptr; },
        Steps(IfElement(
            source,
            [](const ui::TrackedElement* element) {
              return element != nullptr;
            },
            Steps(std::move(step), Log("[KOMB] Added ", step_name,
                                       " with targets: ", dest.GetName(), " ",
                                       source.GetName())),
            Steps(DragMouseTo(dest),  // Dest but no source
                  Log("Added DragMouseTo with target: ", dest.GetName())))),
        Steps(Log("[KOMB] Failed to add step: ", step_name,
                  " Dest:", dest.GetName(), " was not visible"))));
  }

  auto ShowBookmarksBar() {
    return Steps(PressButton(kAppMenuButtonElementId),
                 SelectMenuItem(AppMenuModel::kBookmarksMenuItem),
                 SelectMenuItem(BookmarkSubMenuModel::kShowBookmarkBarMenuItem),
                 WaitForShow(kBookmarkBarElementId));
  }

  // Both ClickRight/ClickLeft are handled by ClickAt in protobuf file
  auto ClickRight(ui::ElementIdentifier target) {
    return Steps(MoveMouseTo(target), ClickMouse(ui_controls::RIGHT));
  }

  auto ClickLeft(ui::ElementIdentifier target) {
    return Steps(MoveMouseTo(target), ClickMouse(ui_controls::LEFT));
  }

  auto DragFromTo(ui::ElementIdentifier source, ui::ElementIdentifier dest) {
    return Steps(MoveMouseTo(source), DragMouseTo(dest));
  }

  // Enum descriptors for protobuf messages
  // Allows for a kombucha verb to function independent of what element
  // it's targeting

  raw_ptr<const google::protobuf::EnumDescriptor> target_descriptor =
      raw_ptr(test::fuzzing::ui_fuzzing::Target_descriptor());
  raw_ptr<const google::protobuf::EnumDescriptor> accelerator_descriptor =
      raw_ptr(test::fuzzing::ui_fuzzing::Accelerator_descriptor());

  ui::Accelerator fullscreen_accelerator_;
  ui::Accelerator close_tab_accelerator_;
  ui::Accelerator group_target_tab_accelerator_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

#if BUILDFLAG(IS_WIN)
  std::unique_ptr<base::win::ScopedCOMInitializer> com_initializer_;
#endif

  base::WeakPtrFactory<KombuchaInProcessFuzzer> weak_ptr_factory_{this};
};

void KombuchaInProcessFuzzer::SetUpOnMainThread() {
  InteractiveBrowserTestT::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
  embedded_test_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&KombuchaInProcessFuzzer::HandleHTTPRequest,
                          weak_ptr_factory_.GetWeakPtr()));
  ASSERT_TRUE(embedded_test_server()->Start());

  // Accelerators for using in fuzzing
  chrome::AcceleratorProviderForBrowser(browser())->GetAcceleratorForCommandId(
      IDC_FULLSCREEN, &fullscreen_accelerator_);
  chrome::AcceleratorProviderForBrowser(browser())->GetAcceleratorForCommandId(
      IDC_CLOSE_TAB, &close_tab_accelerator_);
  chrome::AcceleratorProviderForBrowser(browser())->GetAcceleratorForCommandId(
      IDC_GROUP_TARGET_TAB, &group_target_tab_accelerator_);
}

std::unique_ptr<net::test_server::HttpResponse>
KombuchaInProcessFuzzer::HandleHTTPRequest(
    base::WeakPtr<KombuchaInProcessFuzzer> fuzzer_weak,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response;
  response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/html");
  FuzzCase response_body;
  // We are running on the embedded test server's thread.
  // We want to ask the fuzzer thread for the latest payload,
  // but there's a risk of UaF if it's being destroyed.
  // We use a weak pointer, but we have to dereference that on the originating
  // thread.
  base::RunLoop run_loop;
  base::RepeatingCallback<void()> get_payload_lambda =
      base::BindLambdaForTesting([&]() {
        KombuchaInProcessFuzzer* fuzzer = fuzzer_weak.get();
        if (fuzzer) {
          response_body = fuzzer->current_fuzz_case_;
        }
        run_loop.Quit();
      });
  content::GetUIThreadTaskRunner()->PostTask(FROM_HERE, get_payload_lambda);
  run_loop.Run();
  std::string proto_debug_str = response_body.DebugString();

  response->set_content(base::StringPrintf(
      "<html><body><h1>hello world</h1><p>%s</p></body></html>",
      proto_debug_str.c_str()));
  response->set_code(net::HTTP_OK);
  return response;
}

int KombuchaInProcessFuzzer::Fuzz(const uint8_t* data, size_t size) {
  FuzzCase fuzz_case;
  fuzz_case.ParseFromArray(data, size);

  current_fuzz_case_ = fuzz_case;

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabElementId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondaryTabElementId);
  GURL test_url = embedded_test_server()->GetURL("/test.html");

  // Base input always used in fuzzer
  // Start with three tabs
  auto ui_input =
      Steps(PressButton(kNewTabButtonElementId),
            InstrumentTab(kPrimaryTabElementId, 0),
            AddInstrumentedTab(kSecondaryTabElementId, GURL("about:blank")),
            Log("[KOMB] Passed initial setup steps"));

  auto input_buffer = Steps(Log("[KOMB] Began procedurally generated inputs"));

  // Action can have arbitrary number of steps
  // Translate and append each step to run at once
  test::fuzzing::ui_fuzzing::Action action = fuzz_case.action();

  AddStep(input_buffer,
          Log("[KOMB] Count of Steps generated: ", action.steps_size()));

  for (int j = 0; j < action.steps_size(); j++) {
    test::fuzzing::ui_fuzzing::Step step = action.steps(j);

    switch (step.step_choice_case()) {
      case test::fuzzing::ui_fuzzing::Step::kClickAt: {
        auto* name =
            target_descriptor->FindValueByNumber(step.click_at().target())
                ->name()
                .c_str();
        ui::ElementIdentifier target = ui::ElementIdentifier::FromName(name);

        AddStep(input_buffer,
                CheckStep(target,
                          step.click_at().right_click() ? ClickRight(target)
                                                        : ClickLeft(target),
                          step.click_at().right_click() ? "ClickRight"
                                                        : "ClickLeft"));
        break;
      }
      case test::fuzzing::ui_fuzzing::Step::kDragFromTo: {
        auto* sname =
            target_descriptor->FindValueByNumber(step.drag_from_to().source())
                ->name()
                .c_str();

        auto* dname =
            target_descriptor->FindValueByNumber(step.drag_from_to().dest())
                ->name()
                .c_str();

        ui::ElementIdentifier source = ui::ElementIdentifier::FromName(sname);
        ui::ElementIdentifier dest = ui::ElementIdentifier::FromName(dname);

        AddStep(input_buffer, CheckStep(source, dest, DragFromTo(source, dest),
                                        "DragFromTo"));
        break;
      }
      case test::fuzzing::ui_fuzzing::Step::kSelectTab: {
        auto index =
            step.select_tab().target() % browser()->tab_strip_model()->count();

        // No need to check step, so we log here instead
        AddStep(input_buffer, Log("[KOMB] Adding SelectTab: ", index));
        AddStep(input_buffer, Steps(SelectTab(kTabStripElementId, index)));

        break;
      }
      case test::fuzzing::ui_fuzzing::Step::kSendAccelerator: {
        switch (step.send_accelerator().target()) {
          case test::fuzzing::ui_fuzzing::Accelerator::fullscreen_accelerator_:
            AddStep(input_buffer, SendAccelerator(kBrowserViewElementId,
                                                  fullscreen_accelerator_));
            break;
          case test::fuzzing::ui_fuzzing::Accelerator::closetab_accelerator_:

            AddStep(input_buffer, SendAccelerator(kBrowserViewElementId,
                                                  close_tab_accelerator_));
            break;
          case test::fuzzing::ui_fuzzing::Accelerator::grouptab_accelerator_:
            AddStep(input_buffer,
                    SendAccelerator(kBrowserViewElementId,
                                    group_target_tab_accelerator_));
            break;
          default:  // Unspecified Value
            break;
        }
        break;
      }
      default:  // Unspecified Value
        break;
    }
  }

  if (action.has_parallel_flag()) {
    // TODO(xrosado) Add InParallel() and AnyOf() case in future
  } else {
    // Join ui_input with input_buffer to one input
    ui_input = Steps(std::move(ui_input), std::move(input_buffer));
  }

  AddStep(ui_input,
          Log("[KOMB] Executed all procedurally generated UI inputs"));

  // Set of inputs always placed at the end
  // Mainly used for debugging and sanity checks
  AddStep(ui_input, Steps(NavigateWebContents(kSecondaryTabElementId, test_url),
                          Log("[KOMB] Passed navigation step")));

  RunTestSequence(std::move(ui_input));

  return 0;
}

REGISTER_IN_PROCESS_FUZZER(KombuchaInProcessFuzzer)
DEFINE_BINARY_PROTO_IN_PROCESS_FUZZER(
    KombuchaInProcessFuzzer::FuzzCase testcase)
