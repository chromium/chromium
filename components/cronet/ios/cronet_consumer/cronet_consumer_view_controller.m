// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "cronet_consumer_view_controller.h"

#import <Cronet/Cronet.h>

@implementation CronetConsumerViewController
#if !defined(__IPHONE_12_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_12_0
{
  UIWebView* _webView;
}

- (void)viewDidLoad {
  self.view.backgroundColor = [UIColor whiteColor];

  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  [button setTitle:@"chromium.org" forState:UIControlStateNormal];
  [button setFrame:CGRectMake(5, 0, 95, 50)];
  [button addTarget:self
                action:@selector(loadChromium)
      forControlEvents:UIControlEventTouchUpInside];
  [self.view addSubview:button];

  _webView = [[UIWebView alloc]
      initWithFrame:CGRectMake(0, 52, self.view.bounds.size.width,
                               self.view.bounds.size.height - 52)];
  [self.view addSubview:_webView];
  _webView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

  [self loadChromium];
}

// Disable the status bar to sidestep all the iOS7 status bar issues.
- (BOOL)prefersStatusBarHidden {
  return YES;
}

- (void)loadChromium {
  [_webView
      loadRequest:[NSURLRequest
                      requestWithURL:
                          [NSURL URLWithString:@"https://www.chromium.org"]]];
}
#endif
@end
