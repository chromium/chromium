// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/bookmark_command_source.h"

#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/commander/commander_backend.h"
#include "chrome/browser/ui/commander/commander_view_model.h"
#include "chrome/browser/ui/commander/fuzzy_finder.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/url_and_title.h"

namespace commander {

namespace {

std::unique_ptr<CommandItem> CreateOpenBookmarkItem(
    const bookmarks::UrlAndTitle& bookmark,
    Browser* browser) {
  auto item = std::make_unique<CommandItem>();
  item->title = bookmark.title;
  item->entity_type = CommandItem::Entity::kBookmark;
  // base::Unretained is safe because commands are reset when a browser is
  // closed.
  item->command = base::BindOnce(&chrome::AddTabAt, base::Unretained(browser),
                                 GURL(bookmark.url), -1, true, absl::nullopt);
  return item;
}

CommandSource::CommandResults GetMatchingBookmarks(
    Browser* browser,
    const std::u16string& input) {
  CommandSource::CommandResults results;
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser->profile());
  // This should have been checked already.
  DCHECK(model && model->loaded());
  std::vector<bookmarks::UrlAndTitle> bookmarks;
  model->GetBookmarks(&bookmarks);
  FuzzyFinder finder(input);
  std::vector<gfx::Range> ranges;
  for (bookmarks::UrlAndTitle& bookmark : bookmarks) {
    double score = finder.Find(bookmark.title, &ranges);
    if (score > 0) {
      auto item = CreateOpenBookmarkItem(bookmark, browser);
      item->score = score;
      item->matched_ranges = ranges;
      results.push_back(std::move(item));
    }
  }
  return results;
}

}  // namespace

BookmarkCommandSource::BookmarkCommandSource() = default;
BookmarkCommandSource::~BookmarkCommandSource() = default;

CommandSource::CommandResults BookmarkCommandSource::GetCommands(
    const std::u16string& input,
    Browser* browser) const {
  CommandSource::CommandResults results;
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser->profile());
  // Just no-op instead of waiting for the model to load, since this isn't
  // a persistent UI surface and they can just try again.
  if (!model || !model->loaded() || !model->HasBookmarks())
    return results;

  FuzzyFinder finder(input);
  std::vector<gfx::Range> ranges;
  // TODO(lgrey): Temporarily using an untranslated string since it's not
  // yet clear which commands will ship.
  std::u16string open_title = u"Open bookmark...";
  double score = finder.Find(open_title, &ranges);
  if (score > 0) {
    auto verb = std::make_unique<CommandItem>(open_title, score, ranges);
    // base::Unretained is safe because commands are cleared on browser close.
    verb->command = std::make_pair(
        open_title,
        base::BindRepeating(&GetMatchingBookmarks, base::Unretained(browser)));
    results.push_back(std::move(verb));
  }
  return results;
}

}  // namespace commander
