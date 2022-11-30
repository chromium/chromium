// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/stats.h"

#include "base/metrics/histogram_functions.h"

namespace query_tiles {
namespace stats {

const char kImagePreloadingHistogram[] =
    "Search.QueryTiles.ImagePreloadingEvent";

const char kHttpResponseCodeHistogram[] =
    "Search.QueryTiles.FetcherHttpResponseCode";

const char kNetErrorCodeHistogram[] = "Search.QueryTiles.FetcherNetErrorCode";

const char kRequestStatusHistogram[] = "Search.QueryTiles.RequestStatus";

const char kGroupStatusHistogram[] = "Search.QueryTiles.GroupStatus";

const char kFirstFlowDurationHistogram[] =
    "Search.QueryTiles.Fetcher.FirstFlowDuration";

const char kFetcherStartHourHistogram[] = "Search.QueryTiles.Fetcher.Start";

const char kPrunedGroupReasonHistogram[] =
    "Search.QueryTiles.Group.PruneReason";

const char kTrendingTileEventHistogram[] =
    "Search.QueryTiles.TrendingTileEvent";

void RecordImageLoading(ImagePreloadingEvent event) {
  base::UmaHistogramEnumeration(kImagePreloadingHistogram, event);
}

void RecordTileFetcherResponseCode(int response_code) {
  base::UmaHistogramSparse(kHttpResponseCodeHistogram, response_code);
}

void RecordTileFetcherNetErrorCode(int error_code) {
  base::UmaHistogramSparse(kNetErrorCodeHistogram, -error_code);
}

void RecordTileRequestStatus(TileInfoRequestStatus status) {
  base::UmaHistogramEnumeration(kRequestStatusHistogram, status);
}

void RecordTileGroupStatus(TileGroupStatus status) {
  base::UmaHistogramEnumeration(kGroupStatusHistogram, status);
}

void RecordFirstFetchFlowDuration(int hours) {
  base::UmaHistogramCounts100(kFirstFlowDurationHistogram, hours);
}

void RecordExplodeOnFetchStarted(int explode_hour) {
  base::UmaHistogramExactLinear(kFetcherStartHourHistogram, explode_hour, 24);
}

void RecordGroupPruned(PrunedGroupReason reason) {
  base::UmaHistogramEnumeration(kPrunedGroupReasonHistogram, reason);
}

void RecordTrendingTileEvent(TrendingTileEvent event) {
  base::UmaHistogramEnumeration(kTrendingTileEventHistogram, event);
}

}  // namespace stats
}  // namespace query_tiles
