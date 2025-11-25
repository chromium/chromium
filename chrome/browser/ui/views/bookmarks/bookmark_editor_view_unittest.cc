// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_editor_view.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/tree/tree_view.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_utils.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

// Base class for bookmark editor tests. Creates a BookmarkModel and populates
// it with test data.
class BookmarkEditorViewTest : public testing::Test,
                               public base::test::WithFeatureOverride {
 public:
  BookmarkEditorViewTest()
      : base::test::WithFeatureOverride(
            switches::kSyncEnableBookmarksInTransportMode) {}

  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();
    model_ = BookmarkModelFactory::GetForBrowserContext(profile_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(model());
    AddTestData();
    if (SyncEnableBookmarksInTransportModeEnabled()) {
      model()->CreateAccountPermanentFolders();
      AddAccountTestData();
    }
  }

 protected:
  bool SyncEnableBookmarksInTransportModeEnabled() {
    return IsParamFeatureEnabled();
  }

  std::string base_path() const { return "file:///c:/tmp/"; }

  const BookmarkNode* GetNode(const std::string& name) {
    return model()->GetMostRecentlyAddedUserNodeForURL(
        GURL(base_path() + name));
  }

  BookmarkNode* GetMutableNode(const std::string& name) {
    return const_cast<BookmarkNode*>(GetNode(name));
  }

  BookmarkEditorView::EditorTreeModel* editor_tree_model() {
    return editor_->tree_model_.get();
  }

  bool AreAccountNodesCreated() {
    if (!SyncEnableBookmarksInTransportModeEnabled()) {
      return false;
    }
    const bookmarks::BookmarkNodesSplitByAccountAndLocal permanent_nodes =
        bookmarks::GetPermanentNodesForDisplay(model());
    return !permanent_nodes.account_nodes.empty();
  }

  BookmarkEditorView::EditorNode* editor_root() {
    return editor_tree_model()->GetRoot();
  }

  BookmarkEditorView::EditorNode* account_editor_node() {
    CHECK(AreAccountNodesCreated());
    return editor_root()->children()[0].get();
  }

  BookmarkEditorView::EditorNode* account_bookmark_bar_editor_node() {
    CHECK(AreAccountNodesCreated());
    return account_editor_node()->children()[0].get();
  }

  BookmarkEditorView::EditorNode* account_other_editor_node() {
    CHECK(AreAccountNodesCreated());
    return account_editor_node()->children()[1].get();
  }

  BookmarkEditorView::EditorNode* local_editor_node() {
    CHECK(AreAccountNodesCreated());
    return editor_root()->children()[1].get();
  }

  BookmarkEditorView::EditorNode* local_bookmark_bar_editor_node() {
    if (AreAccountNodesCreated()) {
      return local_editor_node()->children()[0].get();
    }
    return editor_root()->children()[0].get();
  }

  BookmarkEditorView::EditorNode* local_other_editor_node() {
    if (AreAccountNodesCreated()) {
      return local_editor_node()->children()[1].get();
    }
    return editor_root()->children()[1].get();
  }

  BookmarkEditorView::EditorNode* selected_editor_node() {
    return editor_tree_model()->AsNode(tree_view()->GetSelectedNode());
  }

  void CreateEditor(
      Profile* profile,
      const BookmarkEditor::EditDetails& details,
      BookmarkEditor::Configuration configuration,
      BookmarkEditor::OnSaveCallback on_save_callback = base::DoNothing()) {
    editor_ = std::make_unique<BookmarkEditorView>(
        profile, details, configuration, std::move(on_save_callback));
  }

  void SetTitleText(const std::u16string& title) {
    editor_->title_tf_->SetText(title);
  }

  void SetURLText(const std::u16string& text) {
    if (editor_->details_.type != BookmarkEditor::EditDetails::NEW_FOLDER) {
      editor_->url_tf_->SetText(text);
    }
  }

  std::u16string_view GetURLText() const {
    return (editor_->details_.type == BookmarkEditor::EditDetails::NEW_FOLDER)
               ? std::u16string_view()
               : editor_->url_tf_->GetText();
  }

  void ApplyEdits() { editor_->ApplyEdits(); }

  void ApplyEdits(BookmarkEditorView::EditorNode* node) {
    editor_->ApplyEdits(node);
  }

  BookmarkEditorView::EditorNode* AddNewFolder(
      BookmarkEditorView::EditorNode* parent) {
    return editor_->AddNewFolder(parent);
  }

  void NewFolder(BookmarkEditorView::EditorNode* node) {
    return editor_->NewFolder(node);
  }

  bool URLTFHasParent() {
    if (editor_->details_.type == BookmarkEditor::EditDetails::NEW_FOLDER) {
      return false;
    }
    return editor_->url_tf_->parent();
  }

  void ExpandAndSelect() { editor_->ExpandAndSelect(); }

  void DeleteNode(base::OnceCallback<bool(const bookmarks::BookmarkNode* node)>
                      non_empty_folder_confirmation_cb) {
    editor_->ExecuteCommandDelete(std::move(non_empty_folder_confirmation_cb));
  }

  bookmarks::BookmarkModel* model() { return model_; }

  views::TreeView* tree_view() const { return editor_->tree_view_; }
  BookmarkEditorView* editor() const { return editor_.get(); }

  // Creates the following structure:
  // bookmark bar node
  //   a
  //   F1
  //    f1a
  //    F11
  //     f11a
  //   F2
  // other node
  //   oa
  //   OF1
  //     of1a
  void AddTestData() {
    const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
    std::string test_base = base_path();
    model()->AddURL(bookmark_bar_node, 0, u"a", GURL(test_base + "a"));
    const BookmarkNode* f1 = model()->AddFolder(bookmark_bar_node, 1, u"F1");
    model()->AddURL(f1, 0, u"f1a", GURL(test_base + "f1a"));
    const BookmarkNode* f11 = model()->AddFolder(f1, 1, u"F11");
    model()->AddURL(f11, 0, u"f11a", GURL(test_base + "f11a"));
    model()->AddFolder(bookmark_bar_node, 2, u"F2");

    // Children of the other node.
    model()->AddURL(model()->other_node(), 0, u"oa", GURL(test_base + "oa"));
    const BookmarkNode* of1 =
        model()->AddFolder(model()->other_node(), 1, u"OF1");
    model()->AddURL(of1, 0, u"of1a", GURL(test_base + "of1a"));
  }

  // Creates the following structure:
  // account bookmark bar node
  //   acc_a
  //   acc_F1
  //    acc_f1a
  //    acc_F11
  //      acc_f11a
  //   acc_F2
  // account other node
  //   acc_oa
  //   acc_OF1
  //     acc_of1a
  void AddAccountTestData() {
    CHECK(AreAccountNodesCreated());
    const BookmarkNode* account_bookmark_bar_node =
        model()->account_bookmark_bar_node();
    std::string test_base = base_path();
    model()->AddURL(account_bookmark_bar_node, 0, u"acc_a",
                    GURL(test_base + "acc_a"));
    const BookmarkNode* f1 =
        model()->AddFolder(account_bookmark_bar_node, 1, u"acc_F1");
    model()->AddURL(f1, 0, u"acc_f1a", GURL(test_base + "acc_f1a"));
    const BookmarkNode* f11 = model()->AddFolder(f1, 1, u"acc_F11");
    model()->AddURL(f11, 0, u"acc_f11a", GURL(test_base + "acc_f11a"));
    model()->AddFolder(account_bookmark_bar_node, 2, u"acc_F2");

    // Children of the other node.
    model()->AddURL(model()->account_other_node(), 0, u"acc_oa",
                    GURL(test_base + "acc_oa"));
    const BookmarkNode* of1 =
        model()->AddFolder(model()->account_other_node(), 1, u"acc_OF1");
    model()->AddURL(of1, 0, u"acc_of1a", GURL(test_base + "acc_of1a"));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<bookmarks::BookmarkModel> model_;
  ChromeTestViewsDelegate<> views_delegate_;
  std::unique_ptr<BookmarkEditorView> editor_;
};

// Makes sure the tree model matches that of the bookmark bar model.
TEST_P(BookmarkEditorViewTest, ModelsMatchWithoutAccountFolders) {
  // Skip this test if account folders are created.
  if (SyncEnableBookmarksInTransportModeEnabled()) {
    GTEST_SKIP();
  }

  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::AddNodeInFolder(
                   nullptr, static_cast<size_t>(-1), GURL(), std::u16string()),
               BookmarkEditorView::SHOW_TREE);
  // The root should have two or three children: bookmark bar, other bookmarks
  // and conditionally mobile bookmarks.
  ASSERT_EQ(model()->mobile_node()->IsVisible() ? 3u : 2u,
            editor_root()->children().size());
  // Verify the tree structure by directly accessing editor nodes, instead of
  // using helper functions.
  const BookmarkEditorView::EditorNode* bookmark_bar =
      editor_root()->children()[0].get();
  const BookmarkEditorView::EditorNode* other =
      editor_root()->children()[1].get();

  // The bookmark bar node should have 2 nodes: folders F1 and F2.
  ASSERT_EQ(2u, bookmark_bar->children().size());
  ASSERT_EQ(u"F1", bookmark_bar->children()[0]->GetTitle());
  ASSERT_EQ(u"F2", bookmark_bar->children()[1]->GetTitle());
  // F1 should have one child, F11
  const BookmarkEditorView::EditorNode* F1 = bookmark_bar->children()[0].get();
  ASSERT_EQ(1u, F1->children().size());
  ASSERT_EQ(u"F11", F1->children()[0]->GetTitle());
  // Other node should have one child (OF1).
  ASSERT_EQ(1u, other->children().size());
  ASSERT_EQ(u"OF1", other->children()[0]->GetTitle());
}

// Makes sure the tree model matches that of the bookmark bar model with account
// permanent folders.
TEST_P(BookmarkEditorViewTest, ModelsMatchWithAccountFolders) {
  // This test requires account folders to be created.
  if (!SyncEnableBookmarksInTransportModeEnabled()) {
    GTEST_SKIP();
  }

  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::AddNodeInFolder(
                   nullptr, static_cast<size_t>(-1), GURL(), std::u16string()),
               BookmarkEditorView::SHOW_TREE);

  // The root should have two children: account and local.
  ASSERT_EQ(2u, editor_root()->children().size());
  BookmarkEditorView::EditorNode* account = editor_root()->children()[0].get();
  BookmarkEditorView::EditorNode* local = editor_root()->children()[1].get();
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BOOKMARKS_ACCOUNT_BOOKMARKS),
            account->GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BOOKMARKS_DEVICE_BOOKMARKS),
            local->GetTitle());

  // The account node should have the following children: bookmark bar, other
  // bookmarks, and conditionally mobile bookmarks.
  ASSERT_EQ(model()->account_mobile_node()->IsVisible() ? 3u : 2u,
            account->children().size());
  const BookmarkEditorView::EditorNode* account_bb =
      account->children()[0].get();
  const BookmarkEditorView::EditorNode* account_other =
      account->children()[1].get();
  // The account bookmark bar node should have 2 child nodes: folders acc_F1 and
  // acc_F2.
  ASSERT_EQ(2u, account_bb->children().size());
  EXPECT_EQ(u"acc_F1", account_bb->children()[0]->GetTitle());
  EXPECT_EQ(u"acc_F2", account_bb->children()[1]->GetTitle());
  // acc_F1 should have one child, acc_F11
  const BookmarkEditorView::EditorNode* acc_F1 =
      account_bb->children()[0].get();
  ASSERT_EQ(1u, acc_F1->children().size());
  EXPECT_EQ(u"acc_F11", acc_F1->children()[0]->GetTitle());
  // Account other node should have one child (acc_OF1).
  ASSERT_EQ(1u, account_other->children().size());
  EXPECT_EQ(u"acc_OF1", account_other->children()[0]->GetTitle());

  // The local node should have the following children: bookmark bar, other
  // bookmarks, and conditionally mobile bookmarks.
  ASSERT_EQ(model()->mobile_node()->IsVisible() ? 3u : 2u,
            local->children().size());
  const BookmarkEditorView::EditorNode* local_bookmark_bar =
      local->children()[0].get();
  const BookmarkEditorView::EditorNode* local_other =
      local->children()[1].get();
  // The local bookmark bar node should have 2 child nodes: folders F1 and F2.
  ASSERT_EQ(2u, local_bookmark_bar->children().size());
  EXPECT_EQ(u"F1", local_bookmark_bar->children()[0]->GetTitle());
  EXPECT_EQ(u"F2", local_bookmark_bar->children()[1]->GetTitle());
  // F1 should have one child, F11
  const BookmarkEditorView::EditorNode* F1 =
      local_bookmark_bar->children()[0].get();
  ASSERT_EQ(1u, F1->children().size());
  EXPECT_EQ(u"F11", F1->children()[0]->GetTitle());
  // Local other node should have one child (OF1).
  ASSERT_EQ(1u, local_other->children().size());
  EXPECT_EQ(u"OF1", local_other->children()[0]->GetTitle());
}

// Changes the title and makes sure parent/visual order doesn't change.
TEST_P(BookmarkEditorViewTest, EditTitleKeepsPosition) {
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("a")),
               BookmarkEditorView::SHOW_TREE);
  SetTitleText(u"new_a");

  ApplyEdits(local_bookmark_bar_editor_node());

  ASSERT_FALSE(model()->bookmark_bar_node()->children().empty());
  const BookmarkNode* new_a =
      model()->bookmark_bar_node()->children().front().get();
  EXPECT_EQ(u"new_a", new_a->GetTitle());
  // The URL shouldn't have changed.
  EXPECT_TRUE(GURL(base_path() + "a") == new_a->url());
}

// Changes the url and makes sure parent/visual order doesn't change.
TEST_P(BookmarkEditorViewTest, EditURLKeepsPosition) {
  base::Time node_time = base::Time::Now() + base::Days(2);
  GetMutableNode("a")->set_date_added(node_time);
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("a")),
               BookmarkEditorView::SHOW_TREE);

  SetURLText(UTF8ToUTF16(GURL(base_path() + "new_a").spec()));

  ApplyEdits(local_bookmark_bar_editor_node());

  ASSERT_FALSE(model()->bookmark_bar_node()->children().empty());
  const BookmarkNode* a =
      model()->bookmark_bar_node()->children().front().get();
  EXPECT_EQ(u"a", a->GetTitle());
  // The URL should have changed.
  EXPECT_TRUE(GURL(base_path() + "new_a") == a->url());
  EXPECT_TRUE(node_time == a->date_added());
}

// Moves 'a' to be a child of the other node.
TEST_P(BookmarkEditorViewTest, ChangeParent) {
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("a")),
               BookmarkEditorView::SHOW_TREE);

  ApplyEdits(local_other_editor_node());

  ASSERT_EQ(3u, model()->other_node()->children().size());
  const BookmarkNode* a = model()->other_node()->children()[2].get();
  EXPECT_EQ(u"a", a->GetTitle());
  EXPECT_TRUE(GURL(base_path() + "a") == a->url());
}

// Moves 'acc_a' to be a child of the account other node.
TEST_P(BookmarkEditorViewTest, ChangeAccountParent) {
  // This test requires account folders to be created.
  if (!SyncEnableBookmarksInTransportModeEnabled()) {
    GTEST_SKIP();
  }

  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("acc_a")),
               BookmarkEditorView::SHOW_TREE);

  ApplyEdits(account_other_editor_node());

  ASSERT_EQ(3u, model()->account_other_node()->children().size());
  const BookmarkNode* acc_a =
      model()->account_other_node()->children()[2].get();
  EXPECT_EQ(u"acc_a", acc_a->GetTitle());
  EXPECT_TRUE(GURL(base_path() + "acc_a") == acc_a->url());
}

// Moves the local bookmark 'oa' to be a child of the account bookmark bar
// node.
TEST_P(BookmarkEditorViewTest, ChangeParentFromLocalToAccount) {
  // This test requires account folders to be created.
  if (!SyncEnableBookmarksInTransportModeEnabled()) {
    GTEST_SKIP();
  }

  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("oa")),
               BookmarkEditorView::SHOW_TREE);

  ApplyEdits(account_bookmark_bar_editor_node());

  ASSERT_EQ(4u, model()->account_bookmark_bar_node()->children().size());
  const BookmarkNode* oa =
      model()->account_bookmark_bar_node()->children()[3].get();
  EXPECT_EQ(u"oa", oa->GetTitle());
  EXPECT_TRUE(GURL(base_path() + "oa") == oa->url());
}

// Moves 'a' to be a child of the other node and changes its url to new_a.
TEST_P(BookmarkEditorViewTest, ChangeParentAndURL) {
  base::Time node_time = base::Time::Now() + base::Days(2);
  GetMutableNode("a")->set_date_added(node_time);
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("a")),
               BookmarkEditorView::SHOW_TREE);

  SetURLText(UTF8ToUTF16(GURL(base_path() + "new_a").spec()));

  ApplyEdits(local_other_editor_node());

  ASSERT_EQ(3u, model()->other_node()->children().size());
  const BookmarkNode* a = model()->other_node()->children()[2].get();
  EXPECT_EQ(u"a", a->GetTitle());
  EXPECT_TRUE(GURL(base_path() + "new_a") == a->url());
  EXPECT_TRUE(node_time == a->date_added());
}

// Creates a new folder and moves a node to it.
TEST_P(BookmarkEditorViewTest, MoveToNewParent) {
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("a")),
               BookmarkEditorView::SHOW_TREE);

  // Create two nodes: "F21" as a child of "F2" and "F211" as a child of "F21".
  BookmarkEditorView::EditorNode* f2 =
      local_bookmark_bar_editor_node()->children()[1].get();
  BookmarkEditorView::EditorNode* f21 = AddNewFolder(f2);
  f21->SetTitle(u"F21");
  BookmarkEditorView::EditorNode* f211 = AddNewFolder(f21);
  f211->SetTitle(u"F211");

  // Parent the node to "F21".
  ApplyEdits(f2);

  // F2 in the model should have two children now: F21 and the node edited.
  ASSERT_EQ(2u, model()->bookmark_bar_node()->children().size());
  const BookmarkNode* mf2 = model()->bookmark_bar_node()->children()[1].get();
  ASSERT_EQ(2u, mf2->children().size());
  // F21 should be first.
  EXPECT_EQ(u"F21", mf2->children()[0]->GetTitle());
  // Then a.
  EXPECT_EQ(u"a", mf2->children()[1]->GetTitle());

  // F21 should have one child, F211.
  const BookmarkNode* mf21 = mf2->children()[0].get();
  ASSERT_EQ(1u, mf21->children().size());
  EXPECT_EQ(u"F211", mf21->children()[0]->GetTitle());
}

// Brings up the editor, creating a new URL on the bookmark bar.
TEST_P(BookmarkEditorViewTest, NewURL) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();

  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::AddNodeInFolder(
                   bookmark_bar_node, 1, GURL(), std::u16string()),
               BookmarkEditorView::SHOW_TREE);

  SetURLText(UTF8ToUTF16(GURL(base_path() + "a").spec()));
  SetTitleText(u"new_a");

  ApplyEdits(local_bookmark_bar_editor_node());

  ASSERT_EQ(4u, bookmark_bar_node->children().size());
  const BookmarkNode* new_node = bookmark_bar_node->children()[1].get();
  EXPECT_EQ(u"new_a", new_node->GetTitle());
  EXPECT_TRUE(GURL(base_path() + "a") == new_node->url());
}

// Brings up the editor with no tree and modifies the url.
TEST_P(BookmarkEditorViewTest, ChangeURLNoTree) {
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(
                   model()->other_node()->children().front().get()),
               BookmarkEditorView::NO_TREE);

  SetURLText(UTF8ToUTF16(GURL(base_path() + "a").spec()));
  SetTitleText(u"new_a");

  ApplyEdits(nullptr);

  ASSERT_EQ(2u, model()->other_node()->children().size());
  const BookmarkNode* new_node =
      model()->other_node()->children().front().get();
  EXPECT_EQ(u"new_a", new_node->GetTitle());
  EXPECT_TRUE(GURL(base_path() + "a") == new_node->url());
}

// Brings up the editor with no tree and modifies only the title.
TEST_P(BookmarkEditorViewTest, ChangeTitleNoTree) {
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(
                   model()->other_node()->children().front().get()),
               BookmarkEditorView::NO_TREE);

  SetTitleText(u"new_a");

  ApplyEdits(nullptr);

  ASSERT_EQ(2u, model()->other_node()->children().size());
  const BookmarkNode* new_node =
      model()->other_node()->children().front().get();
  EXPECT_EQ(u"new_a", new_node->GetTitle());
}

// Edits the bookmark and ensures resulting URL keeps the same scheme, even
// when userinfo is present in the URL
TEST_P(BookmarkEditorViewTest, EditKeepsScheme) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();

  const GURL kUrl = GURL("http://javascript:scripttext@example.com/");

  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::AddNodeInFolder(
                   bookmark_bar_node, 1, kUrl, std::u16string()),
               BookmarkEditorView::SHOW_TREE);

  // We expect only the trailing / to be trimmed when userinfo is present
  EXPECT_EQ(ASCIIToUTF16(kUrl.spec()), base::StrCat({GetURLText(), u"/"}));

  const std::u16string& kTitle = u"EditingKeepsScheme";
  SetTitleText(kTitle);

  ApplyEdits(local_bookmark_bar_editor_node());

  ASSERT_EQ(4u, bookmark_bar_node->children().size());
  const BookmarkNode* new_node = bookmark_bar_node->children()[1].get();
  EXPECT_EQ(kTitle, new_node->GetTitle());
  EXPECT_EQ(kUrl, new_node->url());
}

// Creates a new folder.
TEST_P(BookmarkEditorViewTest, NewFolder) {
  const BookmarkNode* bookmark_bar_node = model()->bookmark_bar_node();
  BookmarkEditor::EditDetails details =
      BookmarkEditor::EditDetails::AddFolder(bookmark_bar_node, 1);
  BookmarkEditor::EditDetails::BookmarkData url_data;
  url_data.title = u"z";
  url_data.url = GURL(base_path() + "x");
  details.bookmark_data.children.push_back(url_data);
  CreateEditor(profile_.get(), details, BookmarkEditorView::SHOW_TREE);

  // The url field shouldn't be visible.
  EXPECT_FALSE(URLTFHasParent());
  SetTitleText(u"new_F");

  ApplyEdits(local_bookmark_bar_editor_node());

  // Make sure the folder was created.
  ASSERT_EQ(4u, bookmark_bar_node->children().size());
  const BookmarkNode* new_node = bookmark_bar_node->children()[1].get();
  EXPECT_EQ(BookmarkNode::FOLDER, new_node->type());
  EXPECT_EQ(u"new_F", new_node->GetTitle());
  // The node should have one child.
  ASSERT_EQ(1u, new_node->children().size());
  const BookmarkNode* new_child = new_node->children()[0].get();
  // Make sure the child url/title match.
  EXPECT_EQ(BookmarkNode::URL, new_child->type());
  EXPECT_EQ(details.bookmark_data.children.at(0).title, new_child->GetTitle());
  EXPECT_EQ(details.bookmark_data.children.at(0).url, new_child->url());
}

// Creates a new folder and selects a different folder for the folder to appear
// in then the editor is initially created showing.
TEST_P(BookmarkEditorViewTest, MoveFolder) {
  BookmarkEditor::EditDetails details = BookmarkEditor::EditDetails::AddFolder(
      model()->bookmark_bar_node(), static_cast<size_t>(-1));
  BookmarkEditor::EditDetails::BookmarkData url_data;
  url_data.title = u"z";
  url_data.url = GURL(base_path() + "x");
  details.bookmark_data.children.push_back(url_data);
  CreateEditor(profile_.get(), details, BookmarkEditorView::SHOW_TREE);

  SetTitleText(u"new_F");

  // Create the folder in the 'other' folder.
  ApplyEdits(local_other_editor_node());

  // Make sure the folder we edited is still there.
  ASSERT_EQ(3u, model()->other_node()->children().size());
  const BookmarkNode* new_node = model()->other_node()->children()[2].get();
  EXPECT_EQ(BookmarkNode::FOLDER, new_node->type());
  EXPECT_EQ(u"new_F", new_node->GetTitle());
  // The node should have one child.
  ASSERT_EQ(1u, new_node->children().size());
  const BookmarkNode* new_child = new_node->children()[0].get();
  // Make sure the child url/title match.
  EXPECT_EQ(BookmarkNode::URL, new_child->type());
  EXPECT_EQ(details.bookmark_data.children.at(0).title, new_child->GetTitle());
  EXPECT_EQ(details.bookmark_data.children.at(0).url, new_child->url());
}

// Verifies the title of a new folder is updated correctly if ApplyEdits() is
// is invoked while focus is still on the text field.
TEST_P(BookmarkEditorViewTest, NewFolderTitleUpdatedOnCommit) {
  ASSERT_EQ(3u, model()->bookmark_bar_node()->children().size());
  const BookmarkNode* parent =
      model()->bookmark_bar_node()->children()[2].get();

  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::AddNodeInFolder(parent, 1, GURL(),
                                                            std::u16string()),
               BookmarkEditorView::SHOW_TREE);
  ExpandAndSelect();

  SetURLText(UTF8ToUTF16(GURL(base_path() + "a").spec()));
  SetTitleText(u"new_a");

  NewFolder(selected_editor_node());
  ASSERT_NE(nullptr, tree_view()->editor());
  tree_view()->editor()->SetText(u"modified");
  ApplyEdits();

  // Verify the new folder was added and title set appropriately.
  ASSERT_EQ(1u, parent->children().size());
  const BookmarkNode* new_folder = parent->children().front().get();
  EXPECT_TRUE(new_folder->is_folder());
  EXPECT_EQ("modified", base::UTF16ToASCII(new_folder->GetTitle()));
}

TEST_P(BookmarkEditorViewTest, DeleteNonEmptyFolder) {
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("f1a")),
               BookmarkEditorView::SHOW_TREE);
  ExpandAndSelect();

  // Select F11, a non-empty folder to be deleted.
  tree_view()->SetSelectedNode(selected_editor_node()->children().back().get());
  ASSERT_EQ("F11",
            base::UTF16ToASCII(tree_view()->GetSelectedNode()->GetTitle()));

  const bookmarks::BookmarkNode* f1 = GetNode("f11a")->parent()->parent();
  ASSERT_EQ(2u, f1->children().size());
  ASSERT_NE(nullptr, GetNode("f11a"));

  // Issue a deletion for F11. Since it's non-empty, it should ask the user for
  // confirmation.
  bool confirmation_requested = false;
  DeleteNode(
      base::BindLambdaForTesting([&](const bookmarks::BookmarkNode* node) {
        confirmation_requested = true;
        // Mimic the user confirming the deletion.
        return true;
      }));
  EXPECT_TRUE(confirmation_requested);
  ApplyEdits();

  // Both F11 and f11a (its child) should have been deleted.
  EXPECT_EQ(1u, f1->children().size());
  EXPECT_EQ(nullptr, GetNode("f11a"));
}

TEST_P(BookmarkEditorViewTest, CancelNonEmptyFolderDeletion) {
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("f1a")),
               BookmarkEditorView::SHOW_TREE);
  ExpandAndSelect();

  // Select F11, a non-empty folder to be deleted.
  tree_view()->SetSelectedNode(selected_editor_node()->children().back().get());
  ASSERT_EQ("F11",
            base::UTF16ToASCII(tree_view()->GetSelectedNode()->GetTitle()));

  const bookmarks::BookmarkNode* f1 = GetNode("f11a")->parent()->parent();
  ASSERT_EQ(2u, f1->children().size());
  ASSERT_NE(nullptr, GetNode("f11a"));

  // Issue a deletion for F11. Since it's non-empty, it should ask the user for
  // confirmation.
  DeleteNode(
      base::BindLambdaForTesting([](const bookmarks::BookmarkNode* node) {
        // Mimic the user cancelling the deletion.
        return false;
      }));
  ApplyEdits();

  // Both F11 and f11a (its child) should have been deleted.
  EXPECT_EQ(1u, f1->children().size());
  EXPECT_NE(nullptr, GetNode("f11a"));
}

TEST_P(BookmarkEditorViewTest, ConcurrentDeleteDuringConfirmationDialog) {
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("f1a")),
               BookmarkEditorView::SHOW_TREE);
  ExpandAndSelect();

  // Select F11, a non-empty folder to be deleted.)
  EXPECT_EQ("F1",
            base::UTF16ToASCII(tree_view()->GetSelectedNode()->GetTitle()));
  ASSERT_EQ(1u, selected_editor_node()->children().size());
  tree_view()->SetSelectedNode(selected_editor_node()->children().back().get());
  EXPECT_EQ("F11",
            base::UTF16ToASCII(tree_view()->GetSelectedNode()->GetTitle()));

  // Issue a deletion for F11. Since it's non-empty, it should ask the user for
  // confirmation.
  ASSERT_NE(nullptr, GetNode("f11a"));
  const bookmarks::BookmarkNode* mf11 = GetNode("f11a")->parent();

  DeleteNode(base::BindLambdaForTesting(
      [=, this](const bookmarks::BookmarkNode* node) {
        // Before the user confirms the deletion, something else (e.g.
        // extension) could delete the very same bookmark.
        this->model()->Remove(
            mf11, bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
        // Mimic the user confirming the deletion.
        return true;
      }));

  // TODO(crbug.com/415757197): this is not invoked in
  // BookmarkEditorView::Reset() for this test. Without such invocation, the
  // selected folder defaults to the first child of the root, which could either
  // be the Bookmark bar editor node, or the "In your Google Account" editor
  // node. ApplyEdits() should not be executed in the second case.
  ExpandAndSelect();

  ApplyEdits();

  EXPECT_EQ(nullptr, GetNode("f11a"));
}

// TODO(crbug.com/41494057): Fix and re-enable or remove if no longer relevant
// for ChromeRefresh2023.
// Add enough new folders to scroll to the bottom of the scroll view. Verify
// that the editor at the end can still be fully visible.
TEST_P(BookmarkEditorViewTest, DISABLED_EditorFullyShown) {
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("oa")),
               BookmarkEditorView::SHOW_TREE);
  editor()->SetBounds(0, 0, 200, 200);

  views::TreeView* tree = tree_view();
  BookmarkEditorView::EditorNode* parent_node =
      editor_root()->children()[1]->children()[0].get();
  // Add more nodes to exceed the height of the viewport.
  do {
    tree->Expand(parent_node);
    parent_node = AddNewFolder(parent_node);
    views::test::RunScheduledLayout(editor());
  } while (tree->bounds().height() <= tree->parent()->bounds().height());

  // Edit the last node which also has the focus.
  tree->StartEditing(parent_node);
  views::Textfield* editor = tree->editor();
  EXPECT_TRUE(editor && editor->GetVisible());
  views::FocusRing* focus_ring = views::FocusRing::Get(editor);
  ASSERT_TRUE(focus_ring);
  views::ScrollView* scroll_view =
      views::ScrollView::GetScrollViewForContents(tree);
  ASSERT_TRUE(scroll_view);
  gfx::Point bottom_right = focus_ring->GetLocalBounds().bottom_right();
  views::View::ConvertPointToTarget(focus_ring, scroll_view, &bottom_right);
  // Confirm the bottom right of the focus ring is also visible.
  EXPECT_TRUE(scroll_view->GetVisibleRect().Contains(bottom_right));
}

// Test the on_save_callback is called if defined
TEST_P(BookmarkEditorViewTest, OnSaveCallbackRunsOnSaveIfDefined) {
  UNCALLED_MOCK_CALLBACK(BookmarkEditor::OnSaveCallback, on_save_callback);

  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("a")),
               BookmarkEditorView::SHOW_TREE, on_save_callback.Get());

  EXPECT_CALL_IN_SCOPE(on_save_callback, Run,
                       ApplyEdits(local_other_editor_node()));
}

TEST_P(BookmarkEditorViewTest, AccessibleProperties) {
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("oa")),
               BookmarkEditorView::SHOW_TREE);
  ui::AXNodeData data;

  ASSERT_TRUE(editor());
  editor()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_BOOKMARK_EDITOR_TITLE),
            data.GetStringAttribute(ax::mojom::StringAttribute::kName));
}

TEST_P(BookmarkEditorViewTest,
       MoveDialog_SaveButtonDisabledForDescendingNodes) {
  const BookmarkNode* F1 = model()->bookmark_bar_node()->children()[1].get();
  EXPECT_EQ(u"F1", F1->GetTitle());
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::MoveNodes(model(), {F1}),
               BookmarkEditorView::SHOW_TREE);
  ExpandAndSelect();

  // By default, the parent of the selected node is chosen as a location. The
  // dialog button should be enabled. The new folder button stays enabled.
  EXPECT_EQ(model()->bookmark_bar_node()->GetTitle(),
            tree_view()->GetSelectedNode()->GetTitle());
  EXPECT_TRUE(editor()->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  EXPECT_TRUE(editor()->IsNewFolderButtonEnabledForTesting());

  // Select F1 (the node itself) as a location. The dialog button should be
  // disabled.
  ASSERT_EQ(2u, selected_editor_node()->children().size());
  tree_view()->SetSelectedNode(selected_editor_node()->children()[0].get());
  EXPECT_EQ(u"F1", tree_view()->GetSelectedNode()->GetTitle());
  EXPECT_FALSE(editor()->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  EXPECT_TRUE(editor()->IsNewFolderButtonEnabledForTesting());

  // Select F11 (a descendant of the node itself) as a location. The dialog
  // button should be disabled.
  ASSERT_EQ(1u, selected_editor_node()->children().size());
  tree_view()->SetSelectedNode(selected_editor_node()->children()[0].get());
  EXPECT_EQ(u"F11", tree_view()->GetSelectedNode()->GetTitle());
  EXPECT_FALSE(editor()->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  EXPECT_TRUE(editor()->IsNewFolderButtonEnabledForTesting());
}

TEST_P(BookmarkEditorViewTest,
       MoveDialog_SaveAndNewFolderButtonDisabledForTitleNodes) {
  // This test requires account folders to be created.
  if (!SyncEnableBookmarksInTransportModeEnabled()) {
    GTEST_SKIP();
  }

  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("f1a")),
               BookmarkEditorView::SHOW_TREE);
  ExpandAndSelect();

  // Select the account title node. The "Save" and "New Folder" button should be
  // disabled.
  tree_view()->SetSelectedNode(account_editor_node());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BOOKMARKS_ACCOUNT_BOOKMARKS),
            tree_view()->GetSelectedNode()->GetTitle());
  EXPECT_FALSE(editor()->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  EXPECT_FALSE(editor()->IsNewFolderButtonEnabledForTesting());

  // Select the account bookmark bar node. The "Save" and "New Folder" button
  // should be enabled.
  tree_view()->SetSelectedNode(account_bookmark_bar_editor_node());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_FOLDER_NAME),
            tree_view()->GetSelectedNode()->GetTitle());
  EXPECT_TRUE(editor()->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  EXPECT_TRUE(editor()->IsNewFolderButtonEnabledForTesting());

  // Select the local title node. The "Save" and "New Folder" button should be
  // disabled.
  tree_view()->SetSelectedNode(local_editor_node());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BOOKMARKS_DEVICE_BOOKMARKS),
            tree_view()->GetSelectedNode()->GetTitle());
  EXPECT_FALSE(editor()->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  EXPECT_FALSE(editor()->IsNewFolderButtonEnabledForTesting());
}

TEST_P(BookmarkEditorViewTest,
       OnlyExpandTrackedNodesWithoutFeatureFlagEnabled) {
  // Configure the local bookmarks bar to be tracked as expanded.
  ScopedListPrefUpdate update(profile_->GetPrefs(),
                              bookmarks::prefs::kBookmarkEditorExpandedNodes);
  base::Value::List& initial_expanded_nodes_list = update.Get();
  initial_expanded_nodes_list.Append(
      base::NumberToString(model()->bookmark_bar_node()->id()));

  // Open the editor with a node saved under the local other bookmarks folder.
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("oa")),
               BookmarkEditorView::SHOW_TREE);
  ExpandAndSelect();

  // The node being edited should always be visible.
  EXPECT_TRUE(GetNode("oa")->IsVisible());

  if (!SyncEnableBookmarksInTransportModeEnabled()) {
    // The local bookmarks bar should still be open in the tree view.
    EXPECT_TRUE(tree_view()->IsExpanded(local_bookmark_bar_editor_node()));
    return;
  }

  // The local bookmarks bar should no longer be open in the tree view.
  EXPECT_FALSE(tree_view()->IsExpanded(local_bookmark_bar_editor_node()));

  // The same behavior is expected with account nodes.
  initial_expanded_nodes_list.Append(
      base::NumberToString(model()->account_bookmark_bar_node()->id()));
  ApplyEdits();
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("acc_oa")),
               BookmarkEditorView::SHOW_TREE);
  ExpandAndSelect();
  EXPECT_TRUE(GetNode("acc_oa")->IsVisible());
  EXPECT_FALSE(tree_view()->IsExpanded(account_bookmark_bar_editor_node()));
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(BookmarkEditorViewTest);
