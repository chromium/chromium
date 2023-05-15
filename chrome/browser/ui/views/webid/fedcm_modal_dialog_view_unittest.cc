// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/fedcm_modal_dialog_view.h"

#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/controls/label.h"

class FedCmModalDialogView;

namespace {

std::vector<std::string> GetChildClassNames(views::View* parent) {
  std::vector<std::string> child_class_names;
  for (views::View* child_view : parent->children()) {
    child_class_names.push_back(child_view->GetClassName());
  }
  return child_class_names;
}

views::View* GetViewWithClassName(views::View* parent,
                                  const std::string& class_name) {
  for (views::View* child_view : parent->children()) {
    if (child_view->GetClassName() == class_name) {
      return child_view;
    }
  }
  return nullptr;
}

}  // namespace

class FedCmModalDialogViewTest : public ChromeViewsTestBase {
 public:
  FedCmModalDialogViewTest() {
    web_contents_ = web_contents_factory_.CreateWebContents(&testing_profile_);
  }

 protected:
  content::WebContents* web_contents() { return web_contents_; }

 private:
  TestingProfile testing_profile_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents>
      web_contents_;  // Owned by `web_contents_factory_`.
};

TEST_F(FedCmModalDialogViewTest, Init) {
  FedCmModalDialogView modal_dialog_view =
      FedCmModalDialogView(web_contents(), GURL(u"https://example.com"),
                           /*observer=*/nullptr);
  views::View* view = modal_dialog_view.GetContentsView();

  const std::vector<views::View*> container = view->children();
  ASSERT_EQ(container.size(), 1u);

  // Check for header and web view.
  const std::vector<views::View*> children = container[0]->children();
  ASSERT_EQ(children.size(), 2u);
  EXPECT_THAT(GetChildClassNames(container[0]),
              testing::ElementsAreArray({"View", "WebView"}));

  // Check origin label in header.
  views::View* header = children[0];
  views::Label* origin_label =
      static_cast<views::Label*>(GetViewWithClassName(header, "Label"));
  ASSERT_TRUE(origin_label);
  EXPECT_EQ(origin_label->GetText(), u"example.com");
}
