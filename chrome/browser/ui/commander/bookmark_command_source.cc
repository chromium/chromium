// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/bookmark_command_source.h"

#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
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

// The minimum size the input should have before the source returns commands to
// open specific bookmarks without the user choosing "Open Bookmark..." first.
// TODO(lgrey): Centralize this constant when more composite commands are added.
size_t constexpr kNounFirstMinimum = 2;

std::unique_ptr<CommandItem> CreateOpenBookmarkItem(
    const bookmarks::UrlAndTitle& bookmark,
    Browser* browser) {
  auto item = std::make_unique<CommandItem>();
  item->title = bookmark.title;
  item->entity_type = CommandItem::Entity::kBookmark;
  // base::Unretained is safe because commands are reset when a browser is
  // closed.
  item->command = base::BindOnce(&chrome::AddTabAt, base::Unretained(browser),
                                 GURL(bookmark.url), -1, true, base::nullopt);
  return item;
}

CommandSource::CommandResults GetMatchingBookmarks(
    Browser* browser,
    const base::string16& input) {
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
    const base::string16& input,
    Browser* browser) const {
  CommandSource::CommandResults results;
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser->profile());
  // Just no-op instead of waiting for the model to load, since this isn't
  // a persistent UI surface and they can just try again.
  if (!model || !model->loaded() || !model->HasBookmarks())
    return results;

  if (input.size() >= kNounFirstMinimum) {
    results = GetMatchingBookmarks(browser, input);
  }

  FuzzyFinder finder(input);
  std::vector<gfx::Range> ranges;
  // TODO(lgrey): Temporarily using an untranslated string since it's not
  // yet clear which commands will ship.
  base::string16 open_title = base::ASCIIToUTF16("Open bookmark...");
  double score = finder.Find(open_title, &ranges);
  if (score > 0) {
    auto verb = std::make_unique<CommandItem>();
    verb->title = open_title;
    verb->score = score;
    verb->matched_ranges = ranges;
    // base::Unretained is safe because commands are cleared on browser close.
    verb->command = std::make_pair(
        open_title,
        base::BindRepeating(&GetMatchingBookmarks, base::Unretained(browser)));
    results.push_back(std::move(verb));
  }
  return results;
}

}  // namespace commander
