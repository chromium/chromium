// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/importer.h"

#include <stddef.h>

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/common/importer/importer_bridge.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "components/user_data_importer/mojom/bookmark_html_parser.mojom.h"

Importer::RawFaviconUsageData::RawFaviconUsageData() = default;

Importer::RawFaviconUsageData::RawFaviconUsageData(
    GURL favicon_url,
    std::set<GURL> urls,
    std::vector<uint8_t> raw_data)
    : favicon_url(std::move(favicon_url)),
      urls(std::move(urls)),
      raw_data(std::move(raw_data)) {}

Importer::RawFaviconUsageData::RawFaviconUsageData(RawFaviconUsageData&&) =
    default;

Importer::RawFaviconUsageData& Importer::RawFaviconUsageData::operator=(
    RawFaviconUsageData&&) = default;

Importer::RawFaviconUsageData::~RawFaviconUsageData() = default;

void Importer::Cancel() {
  cancelled_.store(true, std::memory_order_relaxed);
}

void Importer::SetBookmarkHtmlParser(
    mojo::PendingRemote<user_data_importer::mojom::BookmarkHtmlParser> parser) {
  bookmark_html_parser_remote_ = std::move(parser);
}

Importer::Importer() = default;

Importer::~Importer() = default;

std::unique_ptr<mojo::Remote<user_data_importer::mojom::BookmarkHtmlParser>>
Importer::BindBookmarkHtmlParser() {
  CHECK(bookmark_html_parser_remote_.is_valid());
  return std::make_unique<
      mojo::Remote<user_data_importer::mojom::BookmarkHtmlParser>>(
      std::move(bookmark_html_parser_remote_));
}

void Importer::ReencodeFaviconsAndSetOnBridge(
    std::vector<RawFaviconUsageData> raw_favicons,
    base::OnceClosure on_done) {
  ReencodeFaviconsAndSetOnBridge(std::move(raw_favicons), std::move(on_done),
                                 nullptr);
}

void Importer::ReencodeFaviconsAndSetOnBridge(
    std::vector<RawFaviconUsageData> raw_favicons,
    base::OnceClosure on_done,
    std::unique_ptr<mojo::Remote<user_data_importer::mojom::BookmarkHtmlParser>>
        parser) {
  if (raw_favicons.empty() || cancelled()) {
    parser.reset();
    bookmark_html_parser_remote_.reset();
    std::move(on_done).Run();
    return;
  }

  if (!parser) {
    parser = BindBookmarkHtmlParser();
  }

  std::vector<std::pair<uint32_t, std::vector<uint8_t>>> raw_data_entries;
  raw_data_entries.reserve(raw_favicons.size());
  for (size_t i = 0; i < raw_favicons.size(); ++i) {
    raw_data_entries.emplace_back(base::checked_cast<uint32_t>(i),
                                  std::move(raw_favicons[i].raw_data));
  }
  base::flat_map<uint32_t, std::vector<uint8_t>> raw_data_map(
      base::sorted_unique, std::move(raw_data_entries));

  auto split_on_done = base::SplitOnceCallback(std::move(on_done));

  auto* raw_parser = parser.get();
  raw_parser->set_disconnect_handler(std::move(split_on_done.first));
  (*raw_parser)
      ->ReencodeFavicons(
          std::move(raw_data_map),
          base::BindOnce(&Importer::OnFaviconsReencoded,
                         base::WrapRefCounted(this), std::move(parser),
                         std::move(raw_favicons),
                         std::move(split_on_done.second)));
}

void Importer::OnFaviconsReencoded(
    std::unique_ptr<mojo::Remote<user_data_importer::mojom::BookmarkHtmlParser>>
        parser,
    std::vector<RawFaviconUsageData> raw_favicons,
    base::OnceClosure on_done,
    const base::flat_map<uint32_t, std::vector<uint8_t>>& png_favicons) {
  parser.reset();
  if (!cancelled()) {
    favicon_base::FaviconUsageDataList favicons;
    favicons.reserve(png_favicons.size());
    for (size_t i = 0; i < raw_favicons.size(); ++i) {
      auto it = png_favicons.find(base::checked_cast<uint32_t>(i));
      if (it == png_favicons.end() || it->second.empty()) {
        continue;
      }
      favicon_base::FaviconUsageData usage_data;
      usage_data.favicon_url = std::move(raw_favicons[i].favicon_url);
      usage_data.urls = std::move(raw_favicons[i].urls);
      usage_data.png_data = it->second;
      favicons.push_back(std::move(usage_data));
    }
    if (!favicons.empty()) {
      bridge_->SetFavicons(favicons);
    }
  }
  std::move(on_done).Run();
}
