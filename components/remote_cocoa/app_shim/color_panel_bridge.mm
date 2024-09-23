// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/color_panel_bridge.h"

#import <Cocoa/Cocoa.h>

#include "skia/ext/skia_utils_mac.h"

namespace {
// The currently active bridge, to which the ColorPanelListener will forward
// its observations.
remote_cocoa::ColorPanelBridge* g_current_panel_bridge = nullptr;
}  // namespace

// A singleton listener class to act as a event target for NSColorPanel and
// send the results to the C++ class, ColorPanelBridge.
@interface ColorPanelListener : NSObject {
 @protected
  // We don't call DidChooseColor if the change wasn't caused by the user
  // interacting with the panel.
  BOOL _nonUserChange;
}
// Called from NSNotificationCenter.
- (void)windowWillClose:(NSNotification*)notification;

// Called from NSColorPanel.
- (void)didChooseColor:(NSColorPanel*)panel;

// The singleton instance.
+ (ColorPanelListener*)instance;

// Show the NSColorPanel.
- (void)showColorPanel;

// Sets color to the NSColorPanel as a non user change.
- (void)setColor:(NSColor*)color;
@end

@implementation ColorPanelListener
- (instancetype)init {
  if ((self = [super init])) {
    NSColorPanel* panel = [NSColorPanel sharedColorPanel];
    NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
    [nc addObserver:self
           selector:@selector(windowWillClose:)
               name:NSWindowWillCloseNotification
             object:panel];
    [nc addObserver:self
           selector:@selector(windowDidResignKey:)
               name:NSWindowDidResignKeyNotification
             object:panel];
  }
  return self;
}

- (void)dealloc {
  // This object is never freed.
  NOTREACHED_IN_MIGRATION();
}

- (void)windowWillClose:(NSNotification*)notification {
  if (g_current_panel_bridge)
    g_current_panel_bridge->host()->DidCloseColorPanel();
  _nonUserChange = NO;
}

- (void)windowDidResignKey:(NSNotification*)notification {
  // Close the color panel when the user clicks away.
  [self windowWillClose:notification];
  [[NSColorPanel sharedColorPanel] close];
}

- (void)didChooseColor:(NSColorPanel*)panel {
  if (_nonUserChange) {
    _nonUserChange = NO;
    return;
  }
  _nonUserChange = NO;
  NSColor* color = panel.color;
  if (color.type == NSColorTypeCatalog) {
    color = [color colorUsingColorSpace:NSColorSpace.genericRGBColorSpace];
    // Some colors in "Developer" palette in "Color Palettes" tab can't be
    // converted to RGB. We just ignore such colors.
    // TODO(tkent): We should notice the rejection to users.
    if (!color)
      return;
  }
  SkColor skColor = 0;
  if (color.colorSpace == NSColorSpace.genericRGBColorSpace) {
    // genericRGB -> deviceRGB conversion isn't ignorable.  We'd like to use RGB
    // values shown in NSColorPanel UI.
    CGFloat red, green, blue, alpha;
    [color getRed:&red green:&green blue:&blue alpha:&alpha];
    skColor = SkColorSetARGB(
        SkScalarRoundToInt(255.0 * alpha), SkScalarRoundToInt(255.0 * red),
        SkScalarRoundToInt(255.0 * green), SkScalarRoundToInt(255.0 * blue));
  } else {
    skColor = skia::NSDeviceColorToSkColor(
        [color colorUsingColorSpace:NSColorSpace.deviceRGBColorSpace]);
  }
  if (g_current_panel_bridge)
    g_current_panel_bridge->host()->DidChooseColorInColorPanel(skColor);
}

+ (ColorPanelListener*)instance {
  static ColorPanelListener* listener = [[ColorPanelListener alloc] init];
  return listener;
}

- (void)showColorPanel {
  NSColorPanel* panel = [NSColorPanel sharedColorPanel];
  [panel setShowsAlpha:NO];
  [panel setTarget:self];
  [panel setAction:@selector(didChooseColor:)];
  [panel makeKeyAndOrderFront:nil];
}

- (void)setColor:(NSColor*)color {
  _nonUserChange = YES;
  [[NSColorPanel sharedColorPanel] setColor:color];
}
@end

namespace remote_cocoa {

ColorPanelBridge::ColorPanelBridge(
    mojo::PendingRemote<mojom::ColorPanelHost> host)
    : host_(std::move(host)) {
  g_current_panel_bridge = this;
}

ColorPanelBridge::~ColorPanelBridge() {
  if (g_current_panel_bridge == this)
    g_current_panel_bridge = nullptr;
}

void ColorPanelBridge::Show(uint32_t initial_color, ShowCallback callback) {
  ColorPanelListener* listener = [ColorPanelListener instance];
  [listener setColor:skia::SkColorToDeviceNSColor(initial_color)];
  [listener showColorPanel];
  std::move(callback).Run();
}

void ColorPanelBridge::SetSelectedColor(uint32_t color,
                                        SetSelectedColorCallback callback) {
  ColorPanelListener* listener = [ColorPanelListener instance];
  [listener setColor:skia::SkColorToDeviceNSColor(color)];
  std::move(callback).Run();
}

}  // namespace remote_cocoa
