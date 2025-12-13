// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "chrome/browser/ui/views/user_education/custom_webui_help_bubble.h"
#include "chrome/browser/ui/views/user_education/custom_webui_help_bubble_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/test/data/grit/webui_test_resources.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/help_bubble/custom_help_bubble.h"
#include "components/user_education/common/help_bubble/help_bubble.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/views/help_bubble_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/browser/web_ui_controller_interface_binder.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/event_modifiers.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/webui/resources/cr_components/help_bubble/custom_help_bubble.mojom.h"
#include "ui/webui/webui_util.h"

namespace {

constexpr char kTestWebUIHost[] = "test-help-bubble";
constexpr char kTestWebUIHostUrl[] = "chrome://test-help-bubble";

BASE_FEATURE(kCustomWebUIHelpBubbleTestFeature,
             "TEST_CustomWebUIHelpBubbleTestFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebViewElementId);

// WebUIController that sets up mojo bindings.
class TestWebUIHelpBubbleController : public TopChromeWebUIController,
                                      public CustomWebUIHelpBubbleController {
 public:
  explicit TestWebUIHelpBubbleController(content::WebUI* web_ui)
      : TopChromeWebUIController(web_ui, true) {
    content::WebUIDataSource* const data_source =
        content::WebUIDataSource::CreateAndAdd(
            web_ui->GetWebContents()->GetBrowserContext(), kTestWebUIHost);
    // NOTE: for production WebUI, you would use
    // `webui::SetupWebUIDataSource()`.
    webui::SetJSModuleDefaults(data_source);
    webui::EnableTrustedTypesCSP(data_source);
    // USAGE NOTE: some WebUI prefer to explicitly bind the HTML file so that
    // incorrect URLs generate a 404.
    data_source->SetDefaultResource(
        IDR_WEBUI_CR_COMPONENTS_HELP_BUBBLE_TEST_CUSTOM_HELP_BUBBLE_HTML);
    data_source->AddResourcePath(
        "custom_help_bubble.mojom-webui.js",
        IDR_WEBUI_CR_COMPONENTS_HELP_BUBBLE_CUSTOM_HELP_BUBBLE_MOJOM_WEBUI_JS);
    data_source->AddResourcePath(
        "test_custom_help_bubble.js",
        IDR_WEBUI_CR_COMPONENTS_HELP_BUBBLE_TEST_CUSTOM_HELP_BUBBLE_JS);
    data_source->AddResourcePath(
        "test_custom_help_bubble.html.js",
        IDR_WEBUI_CR_COMPONENTS_HELP_BUBBLE_TEST_CUSTOM_HELP_BUBBLE_HTML_JS);
  }
  ~TestWebUIHelpBubbleController() override = default;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

DECLARE_TOP_CHROME_WEBUI_CONFIG(TestWebUIHelpBubbleController, kTestWebUIHost);
DEFINE_TOP_CHROME_WEBUI_CONFIG(TestWebUIHelpBubbleController)

WEB_UI_CONTROLLER_TYPE_IMPL(TestWebUIHelpBubbleController)

class TestWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  TestWebUIControllerFactory() = default;
  ~TestWebUIControllerFactory() override = default;

  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override {
    if (url.SchemeIs(content::kChromeUIScheme) &&
        url.host() == kTestWebUIHost) {
      return std::make_unique<TestWebUIHelpBubbleController>(web_ui);
    }

    return nullptr;
  }

  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override {
    if (url.SchemeIs(content::kChromeUIScheme)) {
      return reinterpret_cast<content::WebUI::TypeID>(1);
    }

    return content::WebUI::kNoWebUI;
  }

  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override {
    return url.SchemeIs(content::kChromeUIScheme);
  }
};

class TestContentBrowserClient : public ChromeContentBrowserClient {
 public:
  TestContentBrowserClient() = default;
  ~TestContentBrowserClient() override = default;

  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override {
    ChromeContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
        render_frame_host, map);

    // NOTE: this replaces all existing help bubble bindings! It is needed
    // because other actual custom help bubbles may have bound this interface
    // in the browser bindings, and there is no way to extend an existing
    // binding in a binder map.
    //
    // This code is copied loosely from
    // `content::RegisterWebUIControllerInterfaceBinder()`.
    using Interface = custom_help_bubble::mojom::CustomHelpBubbleHandlerFactory;
    map->Add<Interface>([](content::RenderFrameHost* host,
                           mojo::PendingReceiver<Interface> receiver) {
      CHECK(!host->GetParentOrOuterDocument());
      CHECK((content::internal::SafeDownCastAndBindInterface<
             Interface, TestWebUIHelpBubbleController>(host, receiver)));
    });
  }
};

}  // namespace

class CustomWebUIHelpBubbleUiTest : public InteractiveFeaturePromoTest {
 public:
  CustomWebUIHelpBubbleUiTest() = default;
  ~CustomWebUIHelpBubbleUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTest::SetUpOnMainThread();

    base::FilePath pak_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &pak_path));
    pak_path = pak_path.AppendASCII("browser_tests.pak");
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_path, ui::kScaleFactorNone);

    content::SetBrowserClientForTesting(&client_);

    content::WebUIConfigMap::GetInstance().AddWebUIConfig(
        std::make_unique<TestWebUIHelpBubbleControllerConfig>());

    RegisterTestFeature(
        browser(),
        user_education::FeaturePromoSpecification::CreateForCustomUi(
            kCustomWebUIHelpBubbleTestFeature, kToolbarAppMenuButtonElementId,
            MakeCustomWebUIHelpBubbleFactoryCallback<
                TestWebUIHelpBubbleController>(GURL(kTestWebUIHostUrl))));
  }

  static auto CheckMostRecentClosedReason(
      const base::Feature& iph_feature,
      std::optional<user_education::FeaturePromoClosedReason> reason) {
    return CheckView(
               kBrowserViewElementId,
               [&iph_feature](BrowserView* browser_view) {
                 auto* const service =
                     UserEducationServiceFactory::GetForBrowserContext(
                         browser_view->GetProfile());
                 const auto data =
                     service->user_education_storage_service().ReadPromoData(
                         iph_feature);
                 return (data && data->show_count)
                            ? std::make_optional(data->last_dismissed_by)
                            : std::nullopt;
               },
               reason)
        .SetDescription("CheckMostRecentClosedReason()");
  }

  static auto CheckSnoozeCount(const base::Feature& iph_feature,
                               int snooze_count) {
    return CheckView(
               kBrowserViewElementId,
               [&iph_feature](BrowserView* browser_view) {
                 auto* const service =
                     UserEducationServiceFactory::GetForBrowserContext(
                         browser_view->GetProfile());
                 const auto data =
                     service->user_education_storage_service().ReadPromoData(
                         iph_feature);
                 return data ? data->snooze_count : 0;
               },
               snooze_count)
        .SetDescription("CheckSnoozeCount()");
  }

  static auto CheckIsDismissed(const base::Feature& iph_feature,
                               bool dismissed) {
    return CheckView(
               kBrowserViewElementId,
               [&iph_feature](BrowserView* browser_view) {
                 auto* const service =
                     UserEducationServiceFactory::GetForBrowserContext(
                         browser_view->GetProfile());
                 const auto data =
                     service->user_education_storage_service().ReadPromoData(
                         iph_feature);
                 return data ? data->is_dismissed : false;
               },
               dismissed)
        .SetDescription("CheckIsDismissed()");
  }

  static auto CheckIsAnchor(ElementSpecifier el, bool is_anchor) {
    return Steps(CheckView(
                     kToolbarAppMenuButtonElementId,
                     [](BrowserAppMenuButton* button) {
                       return button->GetProperty(
                           user_education::kHasInProductHelpPromoKey);
                     },
                     is_anchor)
                     .SetDescription("Check IPH key property."),
                 CheckView(
                     kToolbarAppMenuButtonElementId,
                     [](BrowserAppMenuButton* button) {
                       return views::InkDrop::Get(button)
                           ->in_attention_state_for_testing();
                     },
                     is_anchor)
                     .SetDescription("Check in attention state."));
  }

  static auto CheckFrame() {
    return Steps(
        CheckView(
            CustomWebUIHelpBubble::kHelpBubbleIdForTesting,
            [](views::BubbleDialogDelegateView* bubble) {
              return bubble->GetBubbleFrameView()->GetDisplayVisibleArrow();
            })
            .SetDescription("Check frame has visible arrow."),
        CheckView(
            CustomWebUIHelpBubble::kHelpBubbleIdForTesting,
            [](views::BubbleDialogDelegateView* bubble) {
              return bubble->GetBubbleFrameView()->background_color();
            },
            GetHelpBubbleDelegate()->GetHelpBubbleBackgroundColorId())
            .SetDescription("Check frame color."));
  }

  const DeepQuery kCancelButton{"test-custom-help-bubble", "#cancel"};
  const DeepQuery kDismissButton{"test-custom-help-bubble", "#dismiss"};
  const DeepQuery kSnoozeButton{"test-custom-help-bubble", "#snooze"};

 private:
  TestWebUIControllerFactory factory_;
  content::ScopedWebUIControllerFactoryRegistration factory_registration_{
      &factory_};
  TestContentBrowserClient client_;
};

IN_PROC_BROWSER_TEST_F(CustomWebUIHelpBubbleUiTest,
                       CreateHelpBubbleAndInteract) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCallbackEvent);
  std::unique_ptr<CustomWebUIHelpBubble> help_bubble;
  base::CallbackListSubscription sub;
  auto context = BrowserUserEducationInterface::From(browser())
                     ->GetUserEducationContextForTesting();
  RunTestSequence(
      CheckElement(
          kToolbarAppMenuButtonElementId,
          [=, &help_bubble, &sub](ui::TrackedElement* el) {
            user_education::FeaturePromoSpecification::BuildHelpBubbleParams
                params;
            params.anchor_element = el;
            help_bubble = CustomWebUIHelpBubble::CreateForController<
                TestWebUIHelpBubbleController>(GURL(kTestWebUIHostUrl), context,
                                               params);
            sub = help_bubble->custom_bubble_ui()->AddUserActionCallback(
                base::BindLambdaForTesting(
                    [=](user_education::CustomHelpBubbleUi::UserAction action) {
                      EXPECT_EQ(user_education::CustomHelpBubbleUi::UserAction::
                                    kCancel,
                                action);
                      ui::ElementTracker::GetFrameworkDelegate()
                          ->NotifyCustomEvent(el, kCallbackEvent);
                    }));
            return help_bubble && help_bubble->is_open();
          }),
      WaitForShow(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      CheckIsAnchor(kToolbarAppMenuButtonElementId, true), CheckFrame(),
      InstrumentNonTabWebView(kWebViewElementId,
                              CustomWebUIHelpBubble::kWebViewIdForTesting),
      ClickElement(kWebViewElementId, kCancelButton),
      WaitForEvent(kToolbarAppMenuButtonElementId, kCallbackEvent),
      Do([&help_bubble]() { help_bubble->Close(); }),
      WaitForHide(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      CheckIsAnchor(kToolbarAppMenuButtonElementId, false));
}

IN_PROC_BROWSER_TEST_F(CustomWebUIHelpBubbleUiTest, ShowPromo_Cancel) {
  RunTestSequence(
      MaybeShowPromo(kCustomWebUIHelpBubbleTestFeature,
                     CustomHelpBubbleShown{
                         CustomWebUIHelpBubble::kHelpBubbleIdForTesting}),
      WaitForShow(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      InstrumentNonTabWebView(kWebViewElementId,
                              CustomWebUIHelpBubble::kWebViewIdForTesting),
      WaitForWebContentsReady(kWebViewElementId, GURL(kTestWebUIHostUrl)),
      // Needs "fire and forget" because dialog is closed in response.
      ClickElement(kWebViewElementId, kCancelButton,
                   ExecuteJsMode::kFireAndForget),
      WaitForHide(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      CheckMostRecentClosedReason(
          kCustomWebUIHelpBubbleTestFeature,
          user_education::FeaturePromoClosedReason::kCancel));
}

IN_PROC_BROWSER_TEST_F(CustomWebUIHelpBubbleUiTest, ShowPromo_Dismiss) {
  RunTestSequence(
      MaybeShowPromo(kCustomWebUIHelpBubbleTestFeature,
                     CustomHelpBubbleShown{
                         CustomWebUIHelpBubble::kHelpBubbleIdForTesting}),
      InstrumentNonTabWebView(kWebViewElementId,
                              CustomWebUIHelpBubble::kWebViewIdForTesting),
      // Needs "fire and forget" because dialog is closed in response.
      ClickElement(kWebViewElementId, kDismissButton,
                   ExecuteJsMode::kFireAndForget),
      WaitForHide(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      CheckMostRecentClosedReason(
          kCustomWebUIHelpBubbleTestFeature,
          user_education::FeaturePromoClosedReason::kDismiss));
}

IN_PROC_BROWSER_TEST_F(CustomWebUIHelpBubbleUiTest, ShowPromo_Snooze) {
  RunTestSequence(
      MaybeShowPromo(kCustomWebUIHelpBubbleTestFeature,
                     CustomHelpBubbleShown{
                         CustomWebUIHelpBubble::kHelpBubbleIdForTesting}),
      InstrumentNonTabWebView(kWebViewElementId,
                              CustomWebUIHelpBubble::kWebViewIdForTesting),
      // Needs "fire and forget" because dialog is closed in response.
      ClickElement(kWebViewElementId, kSnoozeButton,
                   ExecuteJsMode::kFireAndForget),
      WaitForHide(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      CheckSnoozeCount(kCustomWebUIHelpBubbleTestFeature, 1));
}

IN_PROC_BROWSER_TEST_F(CustomWebUIHelpBubbleUiTest, ShowPromo_PressEsc) {
  const views::Widget* widget = nullptr;
  RunTestSequence(
      MaybeShowPromo(kCustomWebUIHelpBubbleTestFeature,
                     CustomHelpBubbleShown{
                         CustomWebUIHelpBubble::kHelpBubbleIdForTesting}),
      InstrumentNonTabWebView(kWebViewElementId,
                              CustomWebUIHelpBubble::kWebViewIdForTesting),
      IfView(
          CustomWebUIHelpBubble::kHelpBubbleIdForTesting,
          [&widget](const views::View* view) {
            widget = view->GetWidget();
            return !widget->IsActive();
          },
          Then(ObserveState(views::test::kCurrentWidgetFocus),
               WaitForState(views::test::kCurrentWidgetFocus, widget))),
      SendAccelerator(CustomWebUIHelpBubble::kHelpBubbleIdForTesting,
                      ui::Accelerator(ui::VKEY_ESCAPE, ui::MODIFIER_NONE)),
      WaitForHide(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      CheckIsDismissed(kCustomWebUIHelpBubbleTestFeature, true));
}

IN_PROC_BROWSER_TEST_F(CustomWebUIHelpBubbleUiTest, ShowPromo_Abort) {
  RunTestSequence(
      MaybeShowPromo(kCustomWebUIHelpBubbleTestFeature,
                     CustomHelpBubbleShown{
                         CustomWebUIHelpBubble::kHelpBubbleIdForTesting}),
      WaitForShow(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      WithView(kBrowserViewElementId,
               [](BrowserView* browser_view) {
                 BrowserUserEducationInterface::From(browser_view->browser())
                     ->AbortFeaturePromo(kCustomWebUIHelpBubbleTestFeature);
               }),
      WaitForHide(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      CheckIsDismissed(kCustomWebUIHelpBubbleTestFeature, false));
}

IN_PROC_BROWSER_TEST_F(CustomWebUIHelpBubbleUiTest, ShowPromo_FeatureUsed) {
  RunTestSequence(
      MaybeShowPromo(kCustomWebUIHelpBubbleTestFeature,
                     CustomHelpBubbleShown{
                         CustomWebUIHelpBubble::kHelpBubbleIdForTesting}),
      WaitForShow(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      WithView(kBrowserViewElementId,
               [](BrowserView* browser_view) {
                 BrowserUserEducationInterface::From(browser_view->browser())
                     ->NotifyFeaturePromoFeatureUsed(
                         kCustomWebUIHelpBubbleTestFeature,
                         FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
               }),
      WaitForHide(CustomWebUIHelpBubble::kHelpBubbleIdForTesting),
      CheckIsDismissed(kCustomWebUIHelpBubbleTestFeature, true));
}
