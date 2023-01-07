// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}

function reportWin(
    auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
  sendReportTo(browserSignals.interestGroupOwner + '/echo?bidder_report');
}
