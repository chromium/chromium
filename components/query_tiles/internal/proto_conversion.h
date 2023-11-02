// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_PROTO_CONVERSION_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_PROTO_CONVERSION_H_

#include "components/query_tiles/internal/tile_group.h"
#include "components/query_tiles/proto/tile.pb.h"
#include "components/query_tiles/proto/tile_response.pb.h"
#include "components/query_tiles/tile.h"

namespace query_tiles {

using ResponseGroupProto = query_tiles::proto::ServerResponse;
using ResponseTileProto = query_tiles::proto::TileInfo;
using TileProto = query_tiles::proto::Tile;
using TileGroupProto = query_tiles::proto::TileGroup;

// Converts a Tile to proto.
void TileToProto(Tile* entry, TileProto* proto);

// Converts a proto to Tile.
void TileFromProto(TileProto* proto, Tile* entry);

// Converts a TileGroup to proto.
void TileGroupToProto(TileGroup* group, TileGroupProto* proto);

// Converts a proto to TileGroup.
void TileGroupFromProto(TileGroupProto* proto, TileGroup* group);

// Converts ServerResponseProto to TileGroup.
void TileGroupFromResponse(const ResponseGroupProto& response,
                           TileGroup* tile_group);

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_PROTO_CONVERSION_H_
