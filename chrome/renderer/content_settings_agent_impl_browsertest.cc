// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/content_settings_agent_impl.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_test_sink.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_view.h"

namespace {

constexpr char kScriptHtml[] = R"HTML(
  <html>
  <head>
    <script src='data:foo'></script>
  </head>
  <body></body>
  </html>;
)HTML";

constexpr char kScriptWithSrcHtml[] = R"HTML(
  <html>
  <head>
    <script src='http://www.example.com/script.js'></script>
  </head>
  <body></body>
  </html>
)HTML";

class MockContentSettingsManagerImpl
    : public chrome::mojom::ContentSettingsManager {
 public:
  struct Log {
    int allow_storage_access_count = 0;
    int on_content_blocked_count = 0;
    ContentSettingsType on_content_blocked_type = ContentSettingsType::DEFAULT;
  };

  explicit MockContentSettingsManagerImpl(Log* log) : log_(log) {}
  ~MockContentSettingsManagerImpl() override = default;

  // chrome::mojom::ContentSettingsManager methods:
  void Clone(mojo::PendingReceiver<chrome::mojom::ContentSettingsManager>
                 receiver) override {
    ADD_FAILURE() << "Not reached";
  }
  void AllowStorageAccess(StorageType storage_type,
                          const url::Origin& origin,
                          const GURL& site_for_cookies,
                          const url::Origin& top_frame_origin,
                          base::OnceCallback<void(bool)> callback) override {
    ++log_->allow_storage_access_count;
    std::move(callback).Run(true);
  }
  void OnContentBlocked(ContentSettingsType type) override {
    ++log_->on_content_blocked_count;
    log_->on_content_blocked_type = type;
  }

 private:
  Log* log_;
};

class MockContentSettingsAgentImpl : public ContentSettingsAgentImpl {
 public:
  MockContentSettingsAgentImpl(content::RenderFrame* render_frame,
                               service_manager::BinderRegistry* registry);
  ~MockContentSettingsAgentImpl() override {}

  const GURL& image_url() const { return image_url_; }
  const std::string& image_origin() const { return image_origin_; }

  // ContentSettingAgentImpl methods:
  void BindContentSettingsManager(
      mojo::Remote<chrome::mojom::ContentSettingsManager>* manager) override;

  int allow_storage_access_count() const {
    return log_.allow_storage_access_count;
  }
  int on_content_blocked_count() const { return log_.on_content_blocked_count; }
  ContentSettingsType on_content_blocked_type() const {
    return log_.on_content_blocked_type;
  }

 private:
  MockContentSettingsManagerImpl::Log log_;
  const GURL image_url_;
  const std::string image_origin_;

  DISALLOW_COPY_AND_ASSIGN(MockContentSettingsAgentImpl);
};

MockContentSettingsAgentImpl::MockContentSettingsAgentImpl(
    content::RenderFrame* render_frame,
    service_manager::BinderRegistry* registry)
    : ContentSettingsAgentImpl(render_frame, false, registry),
      image_url_("http://www.foo.com/image.jpg"),
      image_origin_("http://www.foo.com") {}

void MockContentSettingsAgentImpl::BindContentSettingsManager(
    mojo::Remote<chrome::mojom::ContentSettingsManager>* manager) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<MockContentSettingsManagerImpl>(&log_),
      manager->BindNewPipeAndPassReceiver());
}

// Evaluates a boolean |predicate| every time a provisional load is committed in
// the given |frame| while the instance of this class is in scope, and verifies
// that the result matches the |expectation|.
class CommitTimeConditionChecker : public content::RenderFrameObserver {
 public:
  using Predicate = base::RepeatingCallback<bool()>;

  CommitTimeConditionChecker(content::RenderFrame* frame,
                             const Predicate& predicate,
                             bool expectation)
      : content::RenderFrameObserver(frame),
        predicate_(predicate),
        expectation_(expectation) {}

 protected:
  // RenderFrameObserver:
  void OnDestruct() override {}
  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override {
    EXPECT_EQ(expectation_, predicate_.Run());
  }

 private:
  const Predicate predicate_;
  const bool expectation_;

  DISALLOW_COPY_AND_ASSIGN(CommitTimeConditionChecker);
};

}  // namespace

class ContentSettingsAgentImplBrowserTest : public ChromeRenderViewTest {
  void SetUp() override {
    ChromeRenderViewTest::SetUp();

    // Set up a fake url loader factory to ensure that script loader can create
    // a WebURLLoader.
    CreateFakeWebURLLoaderFactory();

    // Unbind the ContentSettingsAgent interface that would be registered by
    // the ContentSettingsAgentImpl created when the render frame is created.
    view_->GetMainRenderFrame()
        ->GetAssociatedInterfaceRegistry()
        ->RemoveInterface(chrome::mojom::ContentSettingsAgent::Name_);
  }
};

TEST_F(ContentSettingsAgentImplBrowserTest, DidBlockContentType) {
  MockContentSettingsAgentImpl mock_agent(view_->GetMainRenderFrame(),
                                          registry_.get());
  mock_agent.DidBlockContentType(ContentSettingsType::COOKIES);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_agent.on_content_blocked_count());
  EXPECT_EQ(ContentSettingsType::COOKIES, mock_agent.on_content_blocked_type());

  // Blocking the same content type a second time shouldn't send a notification.
  mock_agent.DidBlockContentType(ContentSettingsType::COOKIES);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_agent.on_content_blocked_count());
}

// Tests that multiple invokations of AllowDOMStorage result in a single IPC.
TEST_F(ContentSettingsAgentImplBrowserTest, AllowDOMStorage) {
  // Load some HTML, so we have a valid security origin.
  LoadHTMLWithUrlOverride("<html></html>", "https://example.com/");
  MockContentSettingsAgentImpl mock_agent(view_->GetMainRenderFrame(),
                                          registry_.get());
  mock_agent.AllowStorage(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_agent.allow_storage_access_count());

  // Accessing localStorage from the same origin again shouldn't result in a
  // new IPC.
  mock_agent.AllowStorage(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_agent.allow_storage_access_count());
}

// Regression test for http://crbug.com/35011
TEST_F(ContentSettingsAgentImplBrowserTest, JSBlockSentAfterPageLoad) {
  MockContentSettingsAgentImpl mock_agent(view_->GetMainRenderFrame(),
                                          registry_.get());

  // 1. Load page with JS.
  const char kHtml[] =
      "<html>"
      "<head>"
      "<script>document.createElement('div');</script>"
      "</head>"
      "<body>"
      "</body>"
      "</html>";
  render_thread_->sink().ClearMessages();
  LoadHTML(kHtml);

  // 2. Block JavaScript.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& script_setting_rules =
      content_setting_rules.script_rules;
  script_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_BLOCK)),
      std::string(), false));
  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(view_->GetMainRenderFrame());
  agent->SetContentSettingRules(&content_setting_rules);

  // Make sure no pending messages are in the queue.
  base::RunLoop().RunUntilIdle();
  render_thread_->sink().ClearMessages();

  const auto HasSentOnContentBlocked =
      [](MockContentSettingsAgentImpl* mock_agent) {
        return mock_agent->on_content_blocked_count() > 0;
      };

  // 3. Reload page. Verify that the notification that javascript was blocked
  // has not yet been sent at the time when the navigation commits.
  CommitTimeConditionChecker checker(
      view_->GetMainRenderFrame(),
      base::Bind(HasSentOnContentBlocked, base::Unretained(&mock_agent)),
      false);

  std::string url_str = "data:text/html;charset=utf-8,";
  url_str.append(kHtml);
  GURL url(url_str);
  Reload(url);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(HasSentOnContentBlocked(&mock_agent));
}

TEST_F(ContentSettingsAgentImplBrowserTest, PluginsTemporarilyAllowed) {
  // Load some HTML.
  LoadHTML("<html>Foo</html>");

  std::string foo_plugin = "foo";
  std::string bar_plugin = "bar";

  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(view_->GetMainRenderFrame());
  EXPECT_FALSE(agent->IsPluginTemporarilyAllowed(foo_plugin));

  // Temporarily allow the "foo" plugin.
  agent->OnLoadBlockedPlugins(foo_plugin);
  EXPECT_TRUE(agent->IsPluginTemporarilyAllowed(foo_plugin));
  EXPECT_FALSE(agent->IsPluginTemporarilyAllowed(bar_plugin));

  // Simulate same document navigation.
  OnSameDocumentNavigation(GetMainFrame(), true);
  EXPECT_TRUE(agent->IsPluginTemporarilyAllowed(foo_plugin));
  EXPECT_FALSE(agent->IsPluginTemporarilyAllowed(bar_plugin));

  // Navigate to a different page.
  LoadHTML("<html>Bar</html>");
  EXPECT_FALSE(agent->IsPluginTemporarilyAllowed(foo_plugin));
  EXPECT_FALSE(agent->IsPluginTemporarilyAllowed(bar_plugin));

  // Temporarily allow all plugins.
  agent->OnLoadBlockedPlugins(std::string());
  EXPECT_TRUE(agent->IsPluginTemporarilyAllowed(foo_plugin));
  EXPECT_TRUE(agent->IsPluginTemporarilyAllowed(bar_plugin));
}

TEST_F(ContentSettingsAgentImplBrowserTest, ImagesBlockedByDefault) {
  MockContentSettingsAgentImpl mock_agent(view_->GetMainRenderFrame(),
                                          registry_.get());

  // Load some HTML.
  LoadHTML("<html>Foo</html>");

  // Set the default image blocking setting.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& image_setting_rules =
      content_setting_rules.image_rules;
  image_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_BLOCK)),
      std::string(), false));

  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(view_->GetMainRenderFrame());
  agent->SetContentSettingRules(&content_setting_rules);
  EXPECT_FALSE(agent->AllowImage(true, mock_agent.image_url()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_agent.on_content_blocked_count());
  EXPECT_EQ(ContentSettingsType::IMAGES, mock_agent.on_content_blocked_type());

  // Create an exception which allows the image.
  image_setting_rules.insert(
      image_setting_rules.begin(),
      ContentSettingPatternSource(
          ContentSettingsPattern::Wildcard(),
          ContentSettingsPattern::FromString(mock_agent.image_origin()),
          base::Value::FromUniquePtrValue(
              content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW)),
          std::string(), false));

  EXPECT_TRUE(agent->AllowImage(true, mock_agent.image_url()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_agent.on_content_blocked_count());
}

TEST_F(ContentSettingsAgentImplBrowserTest, ImagesAllowedByDefault) {
  MockContentSettingsAgentImpl mock_agent(view_->GetMainRenderFrame(),
                                          registry_.get());

  // Load some HTML.
  LoadHTML("<html>Foo</html>");

  // Set the default image blocking setting.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& image_setting_rules =
      content_setting_rules.image_rules;
  image_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW)),
      std::string(), false));

  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(view_->GetMainRenderFrame());
  agent->SetContentSettingRules(&content_setting_rules);
  EXPECT_TRUE(agent->AllowImage(true, mock_agent.image_url()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, mock_agent.on_content_blocked_count());

  // Create an exception which blocks the image.
  image_setting_rules.insert(
      image_setting_rules.begin(),
      ContentSettingPatternSource(
          ContentSettingsPattern::Wildcard(),
          ContentSettingsPattern::FromString(mock_agent.image_origin()),
          base::Value::FromUniquePtrValue(
              content_settings::ContentSettingToValue(CONTENT_SETTING_BLOCK)),
          std::string(), false));
  EXPECT_FALSE(agent->AllowImage(true, mock_agent.image_url()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_agent.on_content_blocked_count());
  EXPECT_EQ(ContentSettingsType::IMAGES, mock_agent.on_content_blocked_type());
}

TEST_F(ContentSettingsAgentImplBrowserTest, ContentSettingsBlockScripts) {
  MockContentSettingsAgentImpl mock_agent(view_->GetMainRenderFrame(),
                                          registry_.get());
  // Set the content settings for scripts.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& script_setting_rules =
      content_setting_rules.script_rules;
  script_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_BLOCK)),
      std::string(), false));

  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(view_->GetMainRenderFrame());
  agent->SetContentSettingRules(&content_setting_rules);

  // Load a page which contains a script.
  LoadHTML(kScriptHtml);

  // Verify that the script was blocked.
  EXPECT_EQ(1, mock_agent.on_content_blocked_count());
}

TEST_F(ContentSettingsAgentImplBrowserTest, ContentSettingsAllowScripts) {
  MockContentSettingsAgentImpl mock_agent(view_->GetMainRenderFrame(),
                                          registry_.get());
  // Set the content settings for scripts.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& script_setting_rules =
      content_setting_rules.script_rules;
  script_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW)),
      std::string(), false));

  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(view_->GetMainRenderFrame());
  agent->SetContentSettingRules(&content_setting_rules);

  // Load a page which contains a script.
  LoadHTML(kScriptHtml);

  // Verify that the script was not blocked.
  EXPECT_EQ(0, mock_agent.on_content_blocked_count());
}

TEST_F(ContentSettingsAgentImplBrowserTest,
       ContentSettingsAllowScriptsWithSrc) {
  MockContentSettingsAgentImpl mock_agent(view_->GetMainRenderFrame(),
                                          registry_.get());
  // Set the content settings for scripts.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& script_setting_rules =
      content_setting_rules.script_rules;
  script_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW)),
      std::string(), false));

  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(view_->GetMainRenderFrame());
  agent->SetContentSettingRules(&content_setting_rules);

  // Load a page which contains a script.
  LoadHTML(kScriptWithSrcHtml);

  // Verify that the script was not blocked.
  EXPECT_EQ(0, mock_agent.on_content_blocked_count());
}

// Regression test for crbug.com/232410: Load a page with JS blocked. Then,
// allow JS and reload the page. In each case, only one of noscript or script
// tags should be enabled, but never both.
TEST_F(ContentSettingsAgentImplBrowserTest, ContentSettingsNoscriptTag) {
  // 1. Block JavaScript.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& script_setting_rules =
      content_setting_rules.script_rules;
  script_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_BLOCK)),
      std::string(), false));

  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(view_->GetMainRenderFrame());
  agent->SetContentSettingRules(&content_setting_rules);

  // 2. Load a page which contains a noscript tag and a script tag. Note that
  // the page doesn't have a body tag.
  const char kHtml[] =
      "<html>"
      "<noscript>JS_DISABLED</noscript>"
      "<script>document.write('JS_ENABLED');</script>"
      "</html>";
  LoadHTML(kHtml);
  EXPECT_NE(
      std::string::npos,
      blink::WebFrameContentDumper::DumpLayoutTreeAsText(
          GetMainFrame(), blink::WebFrameContentDumper::kLayoutAsTextNormal)
          .Utf8()
          .find("JS_DISABLED"));
  EXPECT_EQ(
      std::string::npos,
      blink::WebFrameContentDumper::DumpLayoutTreeAsText(
          GetMainFrame(), blink::WebFrameContentDumper::kLayoutAsTextNormal)
          .Utf8()
          .find("JS_ENABLED"));

  // 3. Allow JavaScript.
  script_setting_rules.clear();
  script_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW)),
      std::string(), false));
  agent->SetContentSettingRules(&content_setting_rules);

  // 4. Reload the page.
  std::string url_str = "data:text/html;charset=utf-8,";
  url_str.append(kHtml);
  GURL url(url_str);
  Reload(url);
  EXPECT_NE(
      std::string::npos,
      blink::WebFrameContentDumper::DumpLayoutTreeAsText(
          GetMainFrame(), blink::WebFrameContentDumper::kLayoutAsTextNormal)
          .Utf8()
          .find("JS_ENABLED"));
  EXPECT_EQ(
      std::string::npos,
      blink::WebFrameContentDumper::DumpLayoutTreeAsText(
          GetMainFrame(), blink::WebFrameContentDumper::kLayoutAsTextNormal)
          .Utf8()
          .find("JS_DISABLED"));
}

// Checks that same document navigations don't update content settings for the
// page.
TEST_F(ContentSettingsAgentImplBrowserTest,
       ContentSettingsSameDocumentNavigation) {
  MockContentSettingsAgentImpl mock_agent(view_->GetMainRenderFrame(),
                                          registry_.get());
  // Load a page which contains a script.
  LoadHTML(kScriptHtml);

  // Verify that the script was not blocked.
  EXPECT_EQ(0, mock_agent.on_content_blocked_count());

  // Block JavaScript.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& script_setting_rules =
      content_setting_rules.script_rules;
  script_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_BLOCK)),
      std::string(), false));

  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(view_->GetMainRenderFrame());
  agent->SetContentSettingRules(&content_setting_rules);

  // The page shouldn't see the change to script blocking setting after a
  // same document navigation.
  OnSameDocumentNavigation(GetMainFrame(), true);
  EXPECT_TRUE(agent->AllowScript(true));
}

TEST_F(ContentSettingsAgentImplBrowserTest, ContentSettingsInterstitialPages) {
  MockContentSettingsAgentImpl mock_agent(view_->GetMainRenderFrame(),
                                          registry_.get());
  // Block scripts.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& script_setting_rules =
      content_setting_rules.script_rules;
  script_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_BLOCK)),
      std::string(), false));
  // Block images.
  ContentSettingsForOneType& image_setting_rules =
      content_setting_rules.image_rules;
  image_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_BLOCK)),
      std::string(), false));

  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(view_->GetMainRenderFrame());
  agent->SetContentSettingRules(&content_setting_rules);
  agent->SetAsInterstitial();

  // Load a page which contains a script.
  LoadHTML(kScriptHtml);

  // Verify that the script was allowed.
  EXPECT_EQ(0, mock_agent.on_content_blocked_count());

  // Verify that images are allowed.
  EXPECT_TRUE(agent->AllowImage(true, mock_agent.image_url()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, mock_agent.on_content_blocked_count());
}

TEST_F(ContentSettingsAgentImplBrowserTest, AutoplayContentSettings) {
  MockContentSettingsAgentImpl mock_agent(view_->GetMainRenderFrame(),
                                          registry_.get());

  // Load some HTML.
  LoadHTML("<html>Foo</html>");

  // Set the default setting.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& autoplay_setting_rules =
      content_setting_rules.autoplay_rules;
  autoplay_setting_rules.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      base::Value::FromUniquePtrValue(
          content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW)),
      std::string(), false));

  ContentSettingsAgentImpl* agent =
      ContentSettingsAgentImpl::Get(view_->GetMainRenderFrame());
  agent->SetContentSettingRules(&content_setting_rules);

  EXPECT_TRUE(agent->AllowAutoplay(false));

  // Add rule to block autoplay.
  autoplay_setting_rules.insert(
      autoplay_setting_rules.begin(),
      ContentSettingPatternSource(
          ContentSettingsPattern::Wildcard(),
          ContentSettingsPattern::Wildcard(),
          base::Value::FromUniquePtrValue(
              content_settings::ContentSettingToValue(CONTENT_SETTING_BLOCK)),
          std::string(), false));

  EXPECT_FALSE(agent->AllowAutoplay(true));
}
