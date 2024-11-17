// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_editor_view.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
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
class BookmarkEditorViewTest : public testing::Test {
 public:
  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();
    bookmarks::test::WaitForBookmarkModelToLoad(model());

    AddTestData();
  }

 protected:
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
    if (editor_->details_.type != BookmarkEditor::EditDetails::NEW_FOLDER)
      editor_->url_tf_->SetText(text);
  }

  std::u16string GetURLText() const {
    if (editor_->details_.type != BookmarkEditor::EditDetails::NEW_FOLDER)
      return editor_->url_tf_->GetText();

    return std::u16string();
  }

  void ApplyEdits() {
    editor_->ApplyEdits();
  }

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
    if (editor_->details_.type == BookmarkEditor::EditDetails::NEW_FOLDER)
      return false;
    return editor_->url_tf_->parent();
  }

  void ExpandAndSelect() {
    editor_->ExpandAndSelect();
  }

  void DeleteNode(base::OnceCallback<bool(const bookmarks::BookmarkNode* node)>
                      non_empty_folder_confirmation_cb) {
    editor_->ExecuteCommandDelete(std::move(non_empty_folder_confirmation_cb));
  }

  BookmarkModel* model() {
    return BookmarkModelFactory::GetForBrowserContext(profile_.get());
  }

  views::TreeView* tree_view() const { return editor_->tree_view_; }
  BookmarkEditorView* editor() const { return editor_.get(); }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

 private:
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
    const BookmarkNode* bb_node = model()->bookmark_bar_node();
    std::string test_base = base_path();
    model()->AddURL(bb_node, 0, u"a", GURL(test_base + "a"));
    const BookmarkNode* f1 = model()->AddFolder(bb_node, 1, u"F1");
    model()->AddURL(f1, 0, u"f1a", GURL(test_base + "f1a"));
    const BookmarkNode* f11 = model()->AddFolder(f1, 1, u"F11");
    model()->AddURL(f11, 0, u"f11a", GURL(test_base + "f11a"));
    model()->AddFolder(bb_node, 2, u"F2");

    // Children of the other node.
    model()->AddURL(model()->other_node(), 0, u"oa", GURL(test_base + "oa"));
    const BookmarkNode* of1 =
        model()->AddFolder(model()->other_node(), 1, u"OF1");
    model()->AddURL(of1, 0, u"of1a", GURL(test_base + "of1a"));
  }

  ChromeTestViewsDelegate<> views_delegate_;
  std::unique_ptr<BookmarkEditorView> editor_;
};

// Makes sure the tree model matches that of the bookmark bar model.
TEST_F(BookmarkEditorViewTest, ModelsMatch) {
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::AddNodeInFolder(
                   nullptr, static_cast<size_t>(-1), GURL(), std::u16string()),
               BookmarkEditorView::SHOW_TREE);
  BookmarkEditorView::EditorNode* editor_root = editor_tree_model()->GetRoot();
  // The root should have two or three children: bookmark bar, other bookmarks
  // and conditionally mobile bookmarks.
  ASSERT_EQ(model()->mobile_node()->IsVisible() ? 3u : 2u,
            editor_root->children().size());

  BookmarkEditorView::EditorNode* bb_node = editor_root->children()[0].get();
  // The root should have 2 nodes: folder F1 and F2.
  ASSERT_EQ(2u, bb_node->children().size());
  ASSERT_EQ(u"F1", bb_node->children()[0]->GetTitle());
  ASSERT_EQ(u"F2", bb_node->children()[1]->GetTitle());

  // F1 should have one child, F11
  ASSERT_EQ(1u, bb_node->children()[0]->children().size());
  ASSERT_EQ(u"F11", bb_node->children()[0]->children()[0]->GetTitle());

  BookmarkEditorView::EditorNode* other_node = editor_root->children()[1].get();
  // Other node should have one child (OF1).
  ASSERT_EQ(1u, other_node->children().size());
  ASSERT_EQ(u"OF1", other_node->children()[0]->GetTitle());
}

// Changes the title and makes sure parent/visual order doesn't change.
TEST_F(BookmarkEditorViewTest, EditTitleKeepsPosition) {
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("a")),
               BookmarkEditorView::SHOW_TREE);
  SetTitleText(u"new_a");

  ApplyEdits(editor_tree_model()->GetRoot()->children().front().get());

  const BookmarkNode* bb_node = model()->bookmark_bar_node();
  ASSERT_EQ(u"new_a", bb_node->children().front()->GetTitle());
  // The URL shouldn't have changed.
  ASSERT_TRUE(GURL(base_path() + "a") == bb_node->children().front()->url());
}

// Changes the url and makes sure parent/visual order doesn't change.
TEST_F(BookmarkEditorViewTest, EditURLKeepsPosition) {
  base::Time node_time = base::Time::Now() + base::Days(2);
  GetMutableNode("a")->set_date_added(node_time);
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("a")),
               BookmarkEditorView::SHOW_TREE);

  SetURLText(UTF8ToUTF16(GURL(base_path() + "new_a").spec()));

  ApplyEdits(editor_tree_model()->GetRoot()->children().front().get());

  const BookmarkNode* bb_node = model()->bookmark_bar_node();
  ASSERT_EQ(u"a", bb_node->children().front()->GetTitle());
  // The URL should have changed.
  ASSERT_TRUE(GURL(base_path() + "new_a") ==
              bb_node->children().front()->url());
  ASSERT_TRUE(node_time == bb_node->children().front()->date_added());
}

// Moves 'a' to be a child of the other node.
TEST_F(BookmarkEditorViewTest, ChangeParent) {
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("a")),
               BookmarkEditorView::SHOW_TREE);

  ApplyEdits(editor_tree_model()->GetRoot()->children()[1].get());

  const BookmarkNode* other_node = model()->other_node();
  ASSERT_EQ(u"a", other_node->children()[2]->GetTitle());
  ASSERT_TRUE(GURL(base_path() + "a") == other_node->children()[2]->url());
}

// Moves 'a' to be a child of the other node and changes its url to new_a.
TEST_F(BookmarkEditorViewTest, ChangeParentAndURL) {
  base::Time node_time = base::Time::Now() + base::Days(2);
  GetMutableNode("a")->set_date_added(node_time);
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("a")),
               BookmarkEditorView::SHOW_TREE);

  SetURLText(UTF8ToUTF16(GURL(base_path() + "new_a").spec()));

  ApplyEdits(editor_tree_model()->GetRoot()->children()[1].get());

  const BookmarkNode* other_node = model()->other_node();
  ASSERT_EQ(u"a", other_node->children()[2]->GetTitle());
  ASSERT_TRUE(GURL(base_path() + "new_a") == other_node->children()[2]->url());
  ASSERT_TRUE(node_time == other_node->children()[2]->date_added());
}

// Creates a new folder and moves a node to it.
TEST_F(BookmarkEditorViewTest, MoveToNewParent) {
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("a")),
               BookmarkEditorView::SHOW_TREE);

  // Create two nodes: "F21" as a child of "F2" and "F211" as a child of "F21".
  BookmarkEditorView::EditorNode* f2 =
      editor_tree_model()->GetRoot()->children()[0]->children()[1].get();
  BookmarkEditorView::EditorNode* f21 = AddNewFolder(f2);
  f21->SetTitle(u"F21");
  BookmarkEditorView::EditorNode* f211 = AddNewFolder(f21);
  f211->SetTitle(u"F211");

  // Parent the node to "F21".
  ApplyEdits(f2);

  const BookmarkNode* bb_node = model()->bookmark_bar_node();
  const BookmarkNode* mf2 = bb_node->children()[1].get();

  // F2 in the model should have two children now: F21 and the node edited.
  ASSERT_EQ(2u, mf2->children().size());
  // F21 should be first.
  ASSERT_EQ(u"F21", mf2->children()[0]->GetTitle());
  // Then a.
  ASSERT_EQ(u"a", mf2->children()[1]->GetTitle());

  // F21 should have one child, F211.
  const BookmarkNode* mf21 = mf2->children()[0].get();
  ASSERT_EQ(1u, mf21->children().size());
  ASSERT_EQ(u"F211", mf21->children()[0]->GetTitle());
}

// Brings up the editor, creating a new URL on the bookmark bar.
TEST_F(BookmarkEditorViewTest, NewURL) {
  const BookmarkNode* bb_node = model()->bookmark_bar_node();

  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::AddNodeInFolder(bb_node, 1, GURL(),
                                                            std::u16string()),
               BookmarkEditorView::SHOW_TREE);

  SetURLText(UTF8ToUTF16(GURL(base_path() + "a").spec()));
  SetTitleText(u"new_a");

  ApplyEdits(editor_tree_model()->GetRoot()->children()[0].get());

  ASSERT_EQ(4u, bb_node->children().size());

  const BookmarkNode* new_node = bb_node->children()[1].get();

  EXPECT_EQ(u"new_a", new_node->GetTitle());
  EXPECT_TRUE(GURL(base_path() + "a") == new_node->url());
}

// Brings up the editor with no tree and modifies the url.
TEST_F(BookmarkEditorViewTest, ChangeURLNoTree) {
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(
                   model()->other_node()->children().front().get()),
               BookmarkEditorView::NO_TREE);

  SetURLText(UTF8ToUTF16(GURL(base_path() + "a").spec()));
  SetTitleText(u"new_a");

  ApplyEdits(nullptr);

  const BookmarkNode* other_node = model()->other_node();
  ASSERT_EQ(2u, other_node->children().size());

  const BookmarkNode* new_node = other_node->children().front().get();

  EXPECT_EQ(u"new_a", new_node->GetTitle());
  EXPECT_TRUE(GURL(base_path() + "a") == new_node->url());
}

// Brings up the editor with no tree and modifies only the title.
TEST_F(BookmarkEditorViewTest, ChangeTitleNoTree) {
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(
                   model()->other_node()->children().front().get()),
               BookmarkEditorView::NO_TREE);

  SetTitleText(u"new_a");

  ApplyEdits(nullptr);

  const BookmarkNode* other_node = model()->other_node();
  ASSERT_EQ(2u, other_node->children().size());

  const BookmarkNode* new_node = other_node->children().front().get();

  EXPECT_EQ(u"new_a", new_node->GetTitle());
}

// Edits the bookmark and ensures resulting URL keeps the same scheme, even
// when userinfo is present in the URL
TEST_F(BookmarkEditorViewTest, EditKeepsScheme) {
  const BookmarkNode* kBBNode = model()->bookmark_bar_node();

  const GURL kUrl = GURL("http://javascript:scripttext@example.com/");

  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::AddNodeInFolder(kBBNode, 1, kUrl,
                                                            std::u16string()),
               BookmarkEditorView::SHOW_TREE);

  // We expect only the trailing / to be trimmed when userinfo is present
  EXPECT_EQ(ASCIIToUTF16(kUrl.spec()), GetURLText() + u"/");

  const std::u16string& kTitle = u"EditingKeepsScheme";
  SetTitleText(kTitle);

  ApplyEdits(editor_tree_model()->GetRoot()->children()[0].get());

  ASSERT_EQ(4u, kBBNode->children().size());

  const BookmarkNode* kNewNode = kBBNode->children()[1].get();

  EXPECT_EQ(kTitle, kNewNode->GetTitle());
  EXPECT_EQ(kUrl, kNewNode->url());
}

// Creates a new folder.
TEST_F(BookmarkEditorViewTest, NewFolder) {
  const BookmarkNode* bb_node = model()->bookmark_bar_node();
  BookmarkEditor::EditDetails details =
      BookmarkEditor::EditDetails::AddFolder(bb_node, 1);
  BookmarkEditor::EditDetails::BookmarkData url_data;
  url_data.title = u"z";
  url_data.url = GURL(base_path() + "x");
  details.bookmark_data.children.push_back(url_data);
  CreateEditor(profile_.get(), details, BookmarkEditorView::SHOW_TREE);

  // The url field shouldn't be visible.
  EXPECT_FALSE(URLTFHasParent());
  SetTitleText(u"new_F");

  ApplyEdits(editor_tree_model()->GetRoot()->children()[0].get());

  // Make sure the folder was created.
  ASSERT_EQ(4u, bb_node->children().size());
  const BookmarkNode* new_node = bb_node->children()[1].get();
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
TEST_F(BookmarkEditorViewTest, MoveFolder) {
  BookmarkEditor::EditDetails details = BookmarkEditor::EditDetails::AddFolder(
      model()->bookmark_bar_node(), static_cast<size_t>(-1));
  BookmarkEditor::EditDetails::BookmarkData url_data;
  url_data.title = u"z";
  url_data.url = GURL(base_path() + "x");
  details.bookmark_data.children.push_back(url_data);
  CreateEditor(profile_.get(), details, BookmarkEditorView::SHOW_TREE);

  SetTitleText(u"new_F");

  // Create the folder in the 'other' folder.
  ApplyEdits(editor_tree_model()->GetRoot()->children()[1].get());

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
TEST_F(BookmarkEditorViewTest, NewFolderTitleUpdatedOnCommit) {
  const BookmarkNode* parent =
      model()->bookmark_bar_node()->children()[2].get();

  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::AddNodeInFolder(parent, 1, GURL(),
                                                            std::u16string()),
               BookmarkEditorView::SHOW_TREE);
  ExpandAndSelect();

  SetURLText(UTF8ToUTF16(GURL(base_path() + "a").spec()));
  SetTitleText(u"new_a");

  NewFolder(editor_tree_model()->AsNode(tree_view()->GetSelectedNode()));
  ASSERT_NE(nullptr, tree_view()->editor());
  tree_view()->editor()->SetText(u"modified");
  ApplyEdits();

  // Verify the new folder was added and title set appropriately.
  ASSERT_EQ(1u, parent->children().size());
  const BookmarkNode* new_folder = parent->children().front().get();
  ASSERT_TRUE(new_folder->is_folder());
  EXPECT_EQ("modified", base::UTF16ToASCII(new_folder->GetTitle()));
}

TEST_F(BookmarkEditorViewTest, DeleteNonEmptyFolder) {
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("f1a")),
               BookmarkEditorView::SHOW_TREE);
  ExpandAndSelect();

  // Select F11, a non-empty folder to be deleted.
  tree_view()->SetSelectedNode(editor_tree_model()
                                   ->AsNode(tree_view()->GetSelectedNode())
                                   ->children()
                                   .back()
                                   .get());
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

TEST_F(BookmarkEditorViewTest, CancelNonEmptyFolderDeletion) {
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("f1a")),
               BookmarkEditorView::SHOW_TREE);
  ExpandAndSelect();

  // Select F11, a non-empty folder to be deleted.
  tree_view()->SetSelectedNode(editor_tree_model()
                                   ->AsNode(tree_view()->GetSelectedNode())
                                   ->children()
                                   .back()
                                   .get());
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

TEST_F(BookmarkEditorViewTest, ConcurrentDeleteDuringConfirmationDialog) {
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("f1a")),
               BookmarkEditorView::SHOW_TREE);
  ExpandAndSelect();

  // Select F11, a non-empty folder to be deleted.
  tree_view()->SetSelectedNode(editor_tree_model()
                                   ->AsNode(tree_view()->GetSelectedNode())
                                   ->children()
                                   .back()
                                   .get());
  ASSERT_EQ("F11",
            base::UTF16ToASCII(tree_view()->GetSelectedNode()->GetTitle()));

  const bookmarks::BookmarkNode* f1 = GetNode("f11a")->parent()->parent();
  ASSERT_EQ(2u, f1->children().size());
  ASSERT_NE(nullptr, GetNode("f11a"));

  // Issue a deletion for F11. Since it's non-empty, it should ask the user for
  // confirmation.
  const bookmarks::BookmarkNode* f11 = GetNode("f11a")->parent();

  DeleteNode(base::BindLambdaForTesting(
      [=, this](const bookmarks::BookmarkNode* node) {
        // Before the user confirms the deletion, something else (e.g.
        // extension) could delete the very same bookmark.
        this->model()->Remove(
            f11, bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
        // Mimic the user confirming the deletion.
        return true;
      }));
  ApplyEdits();

  EXPECT_EQ(nullptr, GetNode("f11a"));
}

// TODO(crbug.com/41494057): Fix and re-enable or remove if no longer relevant
// for ChromeRefresh2023.
// Add enough new folders to scroll to the bottom of the scroll view. Verify
// that the editor at the end can still be fully visible.
TEST_F(BookmarkEditorViewTest, DISABLED_EditorFullyShown) {
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("oa")),
               BookmarkEditorView::SHOW_TREE);
  editor()->SetBounds(0, 0, 200, 200);

  views::TreeView* tree = tree_view();
  BookmarkEditorView::EditorNode* parent_node =
      editor_tree_model()->GetRoot()->children()[1]->children()[0].get();
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
TEST_F(BookmarkEditorViewTest, OnSaveCallbackRunsOnSaveIfDefined) {
  UNCALLED_MOCK_CALLBACK(BookmarkEditor::OnSaveCallback, on_save_callback);

  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("a")),
               BookmarkEditorView::SHOW_TREE, on_save_callback.Get());

  EXPECT_CALL_IN_SCOPE(
      on_save_callback, Run,
      ApplyEdits(editor_tree_model()->GetRoot()->children()[1].get()));
}

TEST_F(BookmarkEditorViewTest, AccessibleProperties) {
  CreateEditor(profile_.get(),
               BookmarkEditor::EditDetails::EditNode(GetNode("oa")),
               BookmarkEditorView::SHOW_TREE);
  ui::AXNodeData data;

  ASSERT_TRUE(editor());
  editor()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_BOOKMARK_EDITOR_TITLE),
            data.GetStringAttribute(ax::mojom::StringAttribute::kName));
}
