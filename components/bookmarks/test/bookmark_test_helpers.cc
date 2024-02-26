// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/test/bookmark_test_helpers.h"

#include <stddef.h>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "url/gurl.h"

namespace bookmarks {
namespace test {

namespace {

// BookmarkLoadObserver is used when blocking until the BookmarkModel finishes
// loading. As soon as the BookmarkModel finishes loading the message loop is
// quit.
class BookmarkLoadObserver : public BaseBookmarkModelObserver {
 public:
  explicit BookmarkLoadObserver(base::OnceClosure quit_task)
      : quit_task_(std::move(quit_task)) {}

  BookmarkLoadObserver(const BookmarkLoadObserver&) = delete;
  BookmarkLoadObserver& operator=(const BookmarkLoadObserver&) = delete;

  ~BookmarkLoadObserver() override = default;

 private:
  // BaseBookmarkModelObserver:
  void BookmarkModelChanged() override {}
  void BookmarkModelLoaded(bool ids_reassigned) override {
    std::move(quit_task_).Run();
  }

  base::OnceClosure quit_task_;
};

// Helper function which does the actual work of creating the nodes for
// a particular level in the hierarchy.
std::string::size_type AddNodesFromString(BookmarkModel* model,
                                          const BookmarkNode* node,
                                          const std::string& model_string,
                                          std::string::size_type start_pos) {
  DCHECK(node);
  size_t index = node->children().size();
  static const std::string folder_tell(":[");
  std::string::size_type end_pos = model_string.find(' ', start_pos);
  while (end_pos != std::string::npos) {
    std::string::size_type part_length = end_pos - start_pos;
    std::string node_name = model_string.substr(start_pos, part_length);
    // Are we at the end of a folder group?
    if (node_name != "]") {
      // No, is it a folder?
      std::string tell;
      if (part_length > 2)
        tell = node_name.substr(part_length - 2, 2);
      if (tell == folder_tell) {
        node_name = node_name.substr(0, part_length - 2);
        const BookmarkNode* new_node =
            model->AddFolder(node, index, base::UTF8ToUTF16(node_name));
        end_pos = AddNodesFromString(model, new_node, model_string,
                                     end_pos + 1);
      } else {
        std::string url_string("http://");
        url_string += std::string(node_name.begin(), node_name.end());
        url_string += ".com";
        model->AddURL(
            node, index, base::UTF8ToUTF16(node_name), GURL(url_string));
        ++end_pos;
      }
      ++index;
      start_pos = end_pos;
      end_pos = model_string.find(' ', start_pos);
    } else {
      ++end_pos;
      break;
    }
  }
  return end_pos;
}

}  // namespace

void WaitForBookmarkModelToLoad(BookmarkModel* model) {
  if (model->loaded())
    return;
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  BookmarkLoadObserver observer(run_loop.QuitClosure());
  model->AddObserver(&observer);
  run_loop.Run();
  model->RemoveObserver(&observer);
  DCHECK(model->loaded());
}

std::string ModelStringFromNode(const BookmarkNode* node) {
  std::string child_string;
  for (const auto& child : node->children()) {
    child_string += base::UTF16ToUTF8(child->GetTitle());
    if (child->is_folder())
      child_string += ":[ " + ModelStringFromNode(child.get()) + "]";
    child_string += ' ';
  }
  return child_string;
}

void AddNodesFromModelString(BookmarkModel* model,
                             const BookmarkNode* node,
                             const std::string& model_string) {
  DCHECK(node);
  std::string::size_type start_pos = 0;
  std::string::size_type end_pos =
      AddNodesFromString(model, node, model_string, start_pos);
  DCHECK(end_pos == std::string::npos);
}

}  // namespace test
}  // namespace bookmarks
