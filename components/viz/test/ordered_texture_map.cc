// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/ordered_texture_map.h"

#include <stddef.h>

#include <algorithm>

#include "base/logging.h"
#include "components/viz/test/test_texture.h"

namespace viz {

OrderedTextureMap::OrderedTextureMap() = default;

OrderedTextureMap::~OrderedTextureMap() = default;

void OrderedTextureMap::Append(GLuint id, scoped_refptr<TestTexture> texture) {
  DCHECK(texture);
  DCHECK(!ContainsId(id));

  textures_[id] = texture;
  ordered_textures_.push_back(id);
}

void OrderedTextureMap::Replace(GLuint id, scoped_refptr<TestTexture> texture) {
  DCHECK(texture);
  DCHECK(ContainsId(id));

  textures_[id] = texture;
}

void OrderedTextureMap::Remove(GLuint id) {
  auto map_it = textures_.find(id);
  // for some test we generate dummy tex id, which are not registered,
  // nothing to remove in that case.
  if (map_it == textures_.end())
    return;
  textures_.erase(map_it);

  auto list_it =
      std::find(ordered_textures_.begin(), ordered_textures_.end(), id);
  DCHECK(list_it != ordered_textures_.end());
  ordered_textures_.erase(list_it);
}

size_t OrderedTextureMap::Size() {
  return ordered_textures_.size();
}

bool OrderedTextureMap::ContainsId(GLuint id) {
  return textures_.find(id) != textures_.end();
}

scoped_refptr<TestTexture> OrderedTextureMap::TextureForId(GLuint id) {
  DCHECK(ContainsId(id));
  scoped_refptr<TestTexture> texture = textures_[id];
  DCHECK(texture);
  return texture;
}

GLuint OrderedTextureMap::IdAt(size_t index) {
  DCHECK(index < Size());
  return ordered_textures_[index];
}

}  // namespace viz
