// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/tile.h"

#include <algorithm>
#include <deque>
#include <map>
#include <sstream>
#include <utility>

namespace query_tiles {
namespace {

void DeepCopyTiles(const Tile& input, Tile* out) {
  DCHECK(out);

  out->id = input.id;
  out->display_text = input.display_text;
  out->query_text = input.query_text;
  out->accessibility_text = input.accessibility_text;
  out->image_metadatas = input.image_metadatas;
  out->search_params = input.search_params;
  out->sub_tiles.clear();
  for (const auto& child : input.sub_tiles) {
    auto entry = std::make_unique<Tile>();
    DeepCopyTiles(*child.get(), entry.get());
    out->sub_tiles.emplace_back(std::move(entry));
  }
}

void SerializeEntry(const Tile* entry, std::stringstream& out) {
  if (!entry)
    return;
  out << "entry id: " << entry->id << " query text: " << entry->query_text
      << "  display text: " << entry->display_text
      << "  accessibility_text: " << entry->accessibility_text << " \n";

  for (const auto& image : entry->image_metadatas)
    out << "image url: " << image.url.possibly_invalid_spec() << " \n";
}

std::string DebugStringInternal(const Tile* root) {
  if (!root)
    return std::string();
  std::stringstream out;
  out << "Entries detail: \n";
  std::map<std::string, std::vector<std::string>> cache;
  std::deque<const Tile*> queue;
  queue.emplace_back(root);
  while (!queue.empty()) {
    size_t size = queue.size();
    for (size_t i = 0; i < size; i++) {
      auto* parent = queue.front();
      SerializeEntry(parent, out);
      queue.pop_front();
      for (size_t j = 0; j < parent->sub_tiles.size(); j++) {
        cache[parent->id].emplace_back(parent->sub_tiles[j]->id);
        queue.emplace_back(parent->sub_tiles[j].get());
      }
    }
  }
  out << "Tree table: \n";
  for (auto& pair : cache) {
    std::string line;
    line += pair.first + " : [";
    std::sort(pair.second.begin(), pair.second.end());
    for (const auto& child : pair.second)
      line += " " + child;
    line += " ]\n";
    out << line;
  }
  return out.str();
}

}  // namespace

ImageMetadata::ImageMetadata() = default;

ImageMetadata::ImageMetadata(const GURL& url) : url(url) {}

ImageMetadata::~ImageMetadata() = default;

ImageMetadata::ImageMetadata(const ImageMetadata& other) = default;

bool ImageMetadata::operator==(const ImageMetadata& other) const {
  return url == other.url;
}

TileStats::TileStats() = default;

TileStats::TileStats(base::Time last_clicked_time, double score)
    : last_clicked_time(last_clicked_time), score(score) {}

TileStats::~TileStats() = default;

TileStats::TileStats(const TileStats& other) = default;

bool TileStats::operator==(const TileStats& other) const {
  return last_clicked_time == other.last_clicked_time && score == other.score;
}

bool Tile::operator==(const Tile& other) const {
  return id == other.id && display_text == other.display_text &&
         query_text == other.query_text &&
         accessibility_text == other.accessibility_text &&
         image_metadatas.size() == other.image_metadatas.size() &&
         sub_tiles.size() == other.sub_tiles.size() &&
         search_params == other.search_params;
}

bool Tile::operator!=(const Tile& other) const {
  return !(*this == other);
}

Tile::Tile(const Tile& other) {
  DeepCopyTiles(other, this);
}

Tile::Tile() = default;

Tile::Tile(Tile&& other) noexcept = default;

Tile::~Tile() = default;

Tile& Tile::operator=(const Tile& other) {
  DeepCopyTiles(other, this);
  return *this;
}

Tile& Tile::operator=(Tile&& other) noexcept = default;

std::string Tile::DebugString() {
  return DebugStringInternal(this);
}

}  // namespace query_tiles
