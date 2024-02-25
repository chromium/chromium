// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Basic generate bid script that offers a bid of 1 using the first ad's
// `renderURL` and, if present, the first adComponent's `renderURL`.
function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                     trustedBiddingSignals, browserSignals) {
  const ad = interestGroup.ads[0];

  // `auctionSignals` controls whether or not component auctions are allowed.
  let allowComponentAuction =
      typeof auctionSignals === 'string' &&
      auctionSignals.includes('bidderAllowsComponentAuction');

  let result = {'ad': ad, 'bid': 1, 'render': ad.renderURL,
                'allowComponentAuction': allowComponentAuction};
  if (interestGroup.adComponents && interestGroup.adComponents[0])
    result.adComponents = [interestGroup.adComponents[0].renderURL];
  return result;
}

function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                   browserSignals) {
  sendReportTo(browserSignals.interestGroupOwner + '/echoall?report_bidder');
  registerAdBeacon({
    'auctionwinner':
        browserSignals.interestGroupOwner.replace('a.test', 'd.test') +
        '/echoall?report_win_beacon',
    'reserved.top_navigation':
        browserSignals.interestGroupOwner.replace('a.test', 'd.test') +
        '/_report_event_server.html',
    'reserved.top_navigation_commit':
        browserSignals.interestGroupOwner.replace('a.test', 'd.test') +
        '/_report_event_server.html',
    'click':
        browserSignals.interestGroupOwner.replace('a.test', 'd.test') +
        '/_report_event_server.html'
  });
  if (typeof privateAggregation !== 'undefined') {
    privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
    privateAggregation.contributeToHistogramOnEvent(
        'auctionwinner', {bucket: 3n, value: 5});
  }
}
