// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_editor_view.h"

#include <string>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/tree/tree_view.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

// Base class for bookmark editor tests. Creates a BookmarkModel and populates
// it with test data.
class BookmarkEditorViewTest : public testing::Test {
 public:
  BookmarkEditorViewTest() : model_(nullptr) {}

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    profile_->CreateBookmarkModel(true);

    model_ = BookmarkModelFactory::GetForBrowserContext(profile_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(model_);

    AddTestData();
  }

 protected:
  std::string base_path() const { return "file:///c:/tmp/"; }

  const BookmarkNode* GetNode(const std::string& name) {
    return model_->GetMostRecentlyAddedUserNodeForURL(GURL(base_path() + name));
  }

  BookmarkNode* GetMutableNode(const std::string& name) {
    return const_cast<BookmarkNode*>(GetNode(name));
  }

  BookmarkEditorView::EditorTreeModel* editor_tree_model() {
    return editor_->tree_model_.get();
  }

  void CreateEditor(Profile* profile,
                    const BookmarkNode* parent,
                    const BookmarkEditor::EditDetails& details,
                    BookmarkEditor::Configuration configuration) {
    editor_ = std::make_unique<BookmarkEditorView>(profile, parent, details,
                                                   configuration);
  }

  void SetTitleText(const base::string16& title) {
    editor_->title_tf_->SetText(title);
  }

  void SetURLText(const base::string16& text) {
    if (editor_->details_.type != BookmarkEditor::EditDetails::NEW_FOLDER)
      editor_->url_tf_->SetText(text);
  }

  base::string16 GetURLText() const {
    if (editor_->details_.type != BookmarkEditor::EditDetails::NEW_FOLDER)
      return editor_->url_tf_->GetText();

    return base::string16();
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

  void NewFolder() {
    return editor_->NewFolder();
  }

  bool URLTFHasParent() {
    if (editor_->details_.type == BookmarkEditor::EditDetails::NEW_FOLDER)
      return false;
    return editor_->url_tf_->parent();
  }

  void ExpandAndSelect() {
    editor_->ExpandAndSelect();
  }

  views::TreeView* tree_view() { return editor_->tree_view_; }

  content::BrowserTaskEnvironment task_environment_;

  BookmarkModel* model_;
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
    const BookmarkNode* bb_node = model_->bookmark_bar_node();
    std::string test_base = base_path();
    model_->AddURL(bb_node, 0, ASCIIToUTF16("a"), GURL(test_base + "a"));
    const BookmarkNode* f1 = model_->AddFolder(bb_node, 1, ASCIIToUTF16("F1"));
    model_->AddURL(f1, 0, ASCIIToUTF16("f1a"), GURL(test_base + "f1a"));
    const BookmarkNode* f11 = model_->AddFolder(f1, 1, ASCIIToUTF16("F11"));
    model_->AddURL(f11, 0, ASCIIToUTF16("f11a"), GURL(test_base + "f11a"));
    model_->AddFolder(bb_node, 2, ASCIIToUTF16("F2"));

    // Children of the other node.
    model_->AddURL(model_->other_node(), 0, ASCIIToUTF16("oa"),
                   GURL(test_base + "oa"));
    const BookmarkNode* of1 =
        model_->AddFolder(model_->other_node(), 1, ASCIIToUTF16("OF1"));
    model_->AddURL(of1, 0, ASCIIToUTF16("of1a"), GURL(test_base + "of1a"));
  }

  ChromeTestViewsDelegate views_delegate_;

  std::unique_ptr<BookmarkEditorView> editor_;
};

// Makes sure the tree model matches that of the bookmark bar model.
TEST_F(BookmarkEditorViewTest, ModelsMatch) {
  CreateEditor(profile_.get(), NULL,
               BookmarkEditor::EditDetails::AddNodeInFolder(
                   NULL, size_t{-1}, GURL(), base::string16()),
               BookmarkEditorView::SHOW_TREE);
  BookmarkEditorView::EditorNode* editor_root = editor_tree_model()->GetRoot();
  // The root should have two or three children: bookmark bar, other bookmarks
  // and conditionally mobile bookmarks.
  ASSERT_EQ(model_->mobile_node()->IsVisible() ? 3u : 2u,
            editor_root->children().size());

  BookmarkEditorView::EditorNode* bb_node = editor_root->children()[0].get();
  // The root should have 2 nodes: folder F1 and F2.
  ASSERT_EQ(2u, bb_node->children().size());
  ASSERT_EQ(ASCIIToUTF16("F1"), bb_node->children()[0]->GetTitle());
  ASSERT_EQ(ASCIIToUTF16("F2"), bb_node->children()[1]->GetTitle());

  // F1 should have one child, F11
  ASSERT_EQ(1u, bb_node->children()[0]->children().size());
  ASSERT_EQ(ASCIIToUTF16("F11"),
            bb_node->children()[0]->children()[0]->GetTitle());

  BookmarkEditorView::EditorNode* other_node = editor_root->children()[1].get();
  // Other node should have one child (OF1).
  ASSERT_EQ(1u, other_node->children().size());
  ASSERT_EQ(ASCIIToUTF16("OF1"), other_node->children()[0]->GetTitle());
}

// Changes the title and makes sure parent/visual order doesn't change.
TEST_F(BookmarkEditorViewTest, EditTitleKeepsPosition) {
  CreateEditor(profile_.get(), NULL,
               BookmarkEditor::EditDetails::EditNode(GetNode("a")),
               BookmarkEditorView::SHOW_TREE);
  SetTitleText(ASCIIToUTF16("new_a"));

  ApplyEdits(editor_tree_model()->GetRoot()->children().front().get());

  const BookmarkNode* bb_node = model_->bookmark_bar_node();
  ASSERT_EQ(ASCIIToUTF16("new_a"), bb_node->children().front()->GetTitle());
  // The URL shouldn't have changed.
  ASSERT_TRUE(GURL(base_path() + "a") == bb_node->children().front()->url());
}

// Changes the url and makes sure parent/visual order doesn't change.
TEST_F(BookmarkEditorViewTest, EditURLKeepsPosition) {
  base::Time node_time = base::Time::Now() + base::TimeDelta::FromDays(2);
  GetMutableNode("a")->set_date_added(node_time);
  CreateEditor(profile_.get(), NULL,
               BookmarkEditor::EditDetails::EditNode(GetNode("a")),
               BookmarkEditorView::SHOW_TREE);

  SetURLText(UTF8ToUTF16(GURL(base_path() + "new_a").spec()));

  ApplyEdits(editor_tree_model()->GetRoot()->children().front().get());

  const BookmarkNode* bb_node = model_->bookmark_bar_node();
  ASSERT_EQ(ASCIIToUTF16("a"), bb_node->children().front()->GetTitle());
  // The URL should have changed.
  ASSERT_TRUE(GURL(base_path() + "new_a") ==
              bb_node->children().front()->url());
  ASSERT_TRUE(node_time == bb_node->children().front()->date_added());
}

// Moves 'a' to be a child of the other node.
TEST_F(BookmarkEditorViewTest, ChangeParent) {
  CreateEditor(profile_.get(), NULL,
               BookmarkEditor::EditDetails::EditNode(GetNode("a")),
               BookmarkEditorView::SHOW_TREE);

  ApplyEdits(editor_tree_model()->GetRoot()->children()[1].get());

  const BookmarkNode* other_node = model_->other_node();
  ASSERT_EQ(ASCIIToUTF16("a"), other_node->children()[2]->GetTitle());
  ASSERT_TRUE(GURL(base_path() + "a") == other_node->children()[2]->url());
}

// Moves 'a' to be a child of the other node and changes its url to new_a.
TEST_F(BookmarkEditorViewTest, ChangeParentAndURL) {
  base::Time node_time = base::Time::Now() + base::TimeDelta::FromDays(2);
  GetMutableNode("a")->set_date_added(node_time);
  CreateEditor(profile_.get(), NULL,
               BookmarkEditor::EditDetails::EditNode(GetNode("a")),
               BookmarkEditorView::SHOW_TREE);

  SetURLText(UTF8ToUTF16(GURL(base_path() + "new_a").spec()));

  ApplyEdits(editor_tree_model()->GetRoot()->children()[1].get());

  const BookmarkNode* other_node = model_->other_node();
  ASSERT_EQ(ASCIIToUTF16("a"), other_node->children()[2]->GetTitle());
  ASSERT_TRUE(GURL(base_path() + "new_a") == other_node->children()[2]->url());
  ASSERT_TRUE(node_time == other_node->children()[2]->date_added());
}

// Creates a new folder and moves a node to it.
TEST_F(BookmarkEditorViewTest, MoveToNewParent) {
  CreateEditor(profile_.get(), NULL,
               BookmarkEditor::EditDetails::EditNode(GetNode("a")),
               BookmarkEditorView::SHOW_TREE);

  // Create two nodes: "F21" as a child of "F2" and "F211" as a child of "F21".
  BookmarkEditorView::EditorNode* f2 =
      editor_tree_model()->GetRoot()->children()[0]->children()[1].get();
  BookmarkEditorView::EditorNode* f21 = AddNewFolder(f2);
  f21->SetTitle(ASCIIToUTF16("F21"));
  BookmarkEditorView::EditorNode* f211 = AddNewFolder(f21);
  f211->SetTitle(ASCIIToUTF16("F211"));

  // Parent the node to "F21".
  ApplyEdits(f2);

  const BookmarkNode* bb_node = model_->bookmark_bar_node();
  const BookmarkNode* mf2 = bb_node->children()[1].get();

  // F2 in the model should have two children now: F21 and the node edited.
  ASSERT_EQ(2u, mf2->children().size());
  // F21 should be first.
  ASSERT_EQ(ASCIIToUTF16("F21"), mf2->children()[0]->GetTitle());
  // Then a.
  ASSERT_EQ(ASCIIToUTF16("a"), mf2->children()[1]->GetTitle());

  // F21 should have one child, F211.
  const BookmarkNode* mf21 = mf2->children()[0].get();
  ASSERT_EQ(1u, mf21->children().size());
  ASSERT_EQ(ASCIIToUTF16("F211"), mf21->children()[0]->GetTitle());
}

// Brings up the editor, creating a new URL on the bookmark bar.
TEST_F(BookmarkEditorViewTest, NewURL) {
  const BookmarkNode* bb_node = model_->bookmark_bar_node();

  CreateEditor(profile_.get(), bb_node,
               BookmarkEditor::EditDetails::AddNodeInFolder(
                   bb_node, 1, GURL(), base::string16()),
               BookmarkEditorView::SHOW_TREE);

  SetURLText(UTF8ToUTF16(GURL(base_path() + "a").spec()));
  SetTitleText(ASCIIToUTF16("new_a"));

  ApplyEdits(editor_tree_model()->GetRoot()->children()[0].get());

  ASSERT_EQ(4u, bb_node->children().size());

  const BookmarkNode* new_node = bb_node->children()[1].get();

  EXPECT_EQ(ASCIIToUTF16("new_a"), new_node->GetTitle());
  EXPECT_TRUE(GURL(base_path() + "a") == new_node->url());
}

// Brings up the editor with no tree and modifies the url.
TEST_F(BookmarkEditorViewTest, ChangeURLNoTree) {
  CreateEditor(profile_.get(), NULL,
               BookmarkEditor::EditDetails::EditNode(
                   model_->other_node()->children().front().get()),
               BookmarkEditorView::NO_TREE);

  SetURLText(UTF8ToUTF16(GURL(base_path() + "a").spec()));
  SetTitleText(ASCIIToUTF16("new_a"));

  ApplyEdits(NULL);

  const BookmarkNode* other_node = model_->other_node();
  ASSERT_EQ(2u, other_node->children().size());

  const BookmarkNode* new_node = other_node->children().front().get();

  EXPECT_EQ(ASCIIToUTF16("new_a"), new_node->GetTitle());
  EXPECT_TRUE(GURL(base_path() + "a") == new_node->url());
}

// Brings up the editor with no tree and modifies only the title.
TEST_F(BookmarkEditorViewTest, ChangeTitleNoTree) {
  CreateEditor(profile_.get(), NULL,
               BookmarkEditor::EditDetails::EditNode(
                   model_->other_node()->children().front().get()),
               BookmarkEditorView::NO_TREE);

  SetTitleText(ASCIIToUTF16("new_a"));

  ApplyEdits(NULL);

  const BookmarkNode* other_node = model_->other_node();
  ASSERT_EQ(2u, other_node->children().size());

  const BookmarkNode* new_node = other_node->children().front().get();

  EXPECT_EQ(ASCIIToUTF16("new_a"), new_node->GetTitle());
}

// Edits the bookmark and ensures resulting URL keeps the same scheme, even
// when userinfo is present in the URL
TEST_F(BookmarkEditorViewTest, EditKeepsScheme) {
  const BookmarkNode* kBBNode = model_->bookmark_bar_node();

  const GURL kUrl = GURL("http://javascript:scripttext@example.com/");

  CreateEditor(profile_.get(), kBBNode,
               BookmarkEditor::EditDetails::AddNodeInFolder(kBBNode, 1, kUrl,
                                                            base::string16()),
               BookmarkEditorView::SHOW_TREE);

  // We expect only the trailing / to be trimmed when userinfo is present
  EXPECT_EQ(ASCIIToUTF16(kUrl.spec()), GetURLText() + ASCIIToUTF16("/"));

  const base::string16& kTitle = ASCIIToUTF16("EditingKeepsScheme");
  SetTitleText(kTitle);

  ApplyEdits(editor_tree_model()->GetRoot()->children()[0].get());

  ASSERT_EQ(4u, kBBNode->children().size());

  const BookmarkNode* kNewNode = kBBNode->children()[1].get();

  EXPECT_EQ(kTitle, kNewNode->GetTitle());
  EXPECT_EQ(kUrl, kNewNode->url());
}

// Creates a new folder.
TEST_F(BookmarkEditorViewTest, NewFolder) {
  const BookmarkNode* bb_node = model_->bookmark_bar_node();
  BookmarkEditor::EditDetails details =
      BookmarkEditor::EditDetails::AddFolder(bb_node, 1);
  details.urls.push_back(std::make_pair(GURL(base_path() + "x"),
                                        ASCIIToUTF16("z")));
  CreateEditor(profile_.get(), bb_node, details, BookmarkEditorView::SHOW_TREE);

  // The url field shouldn't be visible.
  EXPECT_FALSE(URLTFHasParent());
  SetTitleText(ASCIIToUTF16("new_F"));

  ApplyEdits(editor_tree_model()->GetRoot()->children()[0].get());

  // Make sure the folder was created.
  ASSERT_EQ(4u, bb_node->children().size());
  const BookmarkNode* new_node = bb_node->children()[1].get();
  EXPECT_EQ(BookmarkNode::FOLDER, new_node->type());
  EXPECT_EQ(ASCIIToUTF16("new_F"), new_node->GetTitle());
  // The node should have one child.
  ASSERT_EQ(1u, new_node->children().size());
  const BookmarkNode* new_child = new_node->children()[0].get();
  // Make sure the child url/title match.
  EXPECT_EQ(BookmarkNode::URL, new_child->type());
  EXPECT_EQ(details.urls[0].second, new_child->GetTitle());
  EXPECT_EQ(details.urls[0].first, new_child->url());
}

// Creates a new folder and selects a different folder for the folder to appear
// in then the editor is initially created showing.
TEST_F(BookmarkEditorViewTest, MoveFolder) {
  BookmarkEditor::EditDetails details = BookmarkEditor::EditDetails::AddFolder(
      model_->bookmark_bar_node(), size_t{-1});
  details.urls.push_back(std::make_pair(GURL(base_path() + "x"),
                                        ASCIIToUTF16("z")));
  CreateEditor(profile_.get(), model_->bookmark_bar_node(),
               details, BookmarkEditorView::SHOW_TREE);

  SetTitleText(ASCIIToUTF16("new_F"));

  // Create the folder in the 'other' folder.
  ApplyEdits(editor_tree_model()->GetRoot()->children()[1].get());

  // Make sure the folder we edited is still there.
  ASSERT_EQ(3u, model_->other_node()->children().size());
  const BookmarkNode* new_node = model_->other_node()->children()[2].get();
  EXPECT_EQ(BookmarkNode::FOLDER, new_node->type());
  EXPECT_EQ(ASCIIToUTF16("new_F"), new_node->GetTitle());
  // The node should have one child.
  ASSERT_EQ(1u, new_node->children().size());
  const BookmarkNode* new_child = new_node->children()[0].get();
  // Make sure the child url/title match.
  EXPECT_EQ(BookmarkNode::URL, new_child->type());
  EXPECT_EQ(details.urls[0].second, new_child->GetTitle());
  EXPECT_EQ(details.urls[0].first, new_child->url());
}

// Verifies the title of a new folder is updated correctly if ApplyEdits() is
// is invoked while focus is still on the text field.
TEST_F(BookmarkEditorViewTest, NewFolderTitleUpdatedOnCommit) {
  const BookmarkNode* parent = model_->bookmark_bar_node()->children()[2].get();

  CreateEditor(profile_.get(), parent,
               BookmarkEditor::EditDetails::AddNodeInFolder(
                   parent, 1, GURL(), base::string16()),
               BookmarkEditorView::SHOW_TREE);
  ExpandAndSelect();

  SetURLText(UTF8ToUTF16(GURL(base_path() + "a").spec()));
  SetTitleText(ASCIIToUTF16("new_a"));

  NewFolder();
  ASSERT_TRUE(tree_view()->editor() != NULL);
  tree_view()->editor()->SetText(ASCIIToUTF16("modified"));
  ApplyEdits();

  // Verify the new folder was added and title set appropriately.
  ASSERT_EQ(1u, parent->children().size());
  const BookmarkNode* new_folder = parent->children().front().get();
  ASSERT_TRUE(new_folder->is_folder());
  EXPECT_EQ("modified", base::UTF16ToASCII(new_folder->GetTitle()));
}
