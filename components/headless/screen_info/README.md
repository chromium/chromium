# Headless `--screen-info` switch

Headless Chrome and Chrome Headless Shell support headless screen configuration using `--screen-info` switch.

The following example starts Chrome in headless mode with a virtual screen that has two displays: a primary one with resolution 1600x1200 pixels, and a secondary one also with resolution 1600x1200 with portrait orientation located to the right of the primary one and scale factor of 2.0


```
./chrome --headless --screen-info={1600x1200}{1200x1600 devicePixelRatio=2.0}
```


## `--screen-info` switch parameters

Screen is defined by a set of space separated parameters enclosed in curly brackets `{}`. There can be more than one screen defined resulting in multiscreen configuration in which the first defined screen is the primary screen.

All screen parameters are optional and assume the default values if omitted. For example, `{}` specifies the default 800x600 primary screen with scale factor 1.0, 24 bit color depth and work area occupying the entire screen.


## Screen origin and size

The first two parameters are positional and specify screen origin and size in the format `X,Y` `WxH`, for example: `{0,0 1024x768}`. Screen origin is defined as the distance from the primary screen top left corner to the top left of the screen. Consequently, the primary screen origin is always at 0,0.

If screen size is omitted the default size is 800x600. If screen origin is omitted, the default origin is 0,0 for the primary screen and adjacent origin to the right of the previous screen for the secondary screens. For example, `{}{}` defines a two screen configuration equivalent to `{0,0 800x600}{800,0 800x600}`.

Screen origin and size are exposed by [ScreenDetailed: left](https://developer.mozilla.org/en-US/docs/Web/API/ScreenDetailed/left), [ScreenDetailed: top](https://developer.mozilla.org/en-US/docs/Web/API/ScreenDetailed/top), [Screen: width](https://developer.mozilla.org/en-US/docs/Web/API/Screen/width) and [Screen: height](https://developer.mozilla.org/en-US/docs/Web/API/Screen/height) properties respectively.


## Screen orientation

[Screen orientation](https://w3c.github.io/screen-orientation/#dom-screen-orientation) is defined by the screen size parameter: if screen width is greater or equal to its height, the orientation is landscape, otherwise it is portrait.

Screen orientation is exposed by the [Screen: orientation](https://developer.mozilla.org/en-US/docs/Web/API/Screen/orientation) property.


## Work area

Screen work area is defined by `workAreaLeft`, `workAreaRight`, `workAreaTop` and `workAreaBottom` parameters. The default value for each of these parameters is 0. The value cannot be negative.

These screen parameters are exposed by the [ScreenDetailed: availLeft](https://developer.mozilla.org/en-US/docs/Web/API/ScreenDetailed/availLeft), [ScreenDetailed: availTop](https://developer.mozilla.org/en-US/docs/Web/API/ScreenDetailed/availTop), [Screen: availWidth](https://developer.mozilla.org/en-US/docs/Web/API/Screen/availWidth) and [Screen: availHeight](https://developer.mozilla.org/en-US/docs/Web/API/Screen/availHeight) properties respectively.


## Color depth

Screen color depth is specified by the `colorDepth` parameter. The default value is 24. The minimal value is 1.

This screen parameter is exposed by the [ScreenDetailed colorDepth](https://developer.mozilla.org/en-US/docs/Web/API/Screen/colorDepth) property or its alias [Screen: pixelDepth](https://developer.mozilla.org/en-US/docs/Web/API/Screen/pixelDepth).


## Device pixel ratio

Device pixel ratio defines the ratio between physical and logical pixels and is specified by the `devicePixelRatio` parameter. The default value is 1.0. The minimal value is 0.5.

This screen parameter is exposed by the [ScreenDetailed devicePixelRatio](https://developer.mozilla.org/en-US/docs/Web/API/ScreenDetailed/devicePixelRatio) property. For example, specifying `{1600x1200 devicePixelRatio=2}` will result in `ScreenDetailed` with the following properties: `width:800 height:600 devicePixelRatio:2`.


## Internal screen

The `isInternal` parameter indicates whether the screen is internal to the device (typically a notebook, tablet or phone screen) or external, connected to the device (typically a wired monitor). The allowed values are `0`, `1`, `false` and `true`. Default is `true`.

This screen parameter is exposed by the [ScreenDetailed: isInternal](https://developer.mozilla.org/en-US/docs/Web/API/ScreenDetailed/isInternal) property.


## Screen label

A user-friendly label for the screen is specified by the `label` parameter. The default value is no label. Should be enclosed in single quotes if it contains spaces.

Example: `{label='Monitor #1'}{label='Monitor #2'}`.

This screen parameter is exposed by the [ScreenDetailed: label](https://developer.mozilla.org/en-US/docs/Web/API/ScreenDetailed/label) property.


## Screen rotation

Screen rotation angle is specified by the `rotation` parameter. The valid values are `0`, `90`, `180` and `270`.

This screen parameter is exposed as [ScreenOrientation: angle](https://developer.mozilla.org/en-US/docs/Web/API/ScreenOrientation/angle) property.
