# Eye Dropper Component

Views implementation of JS [EyeDropper](https://wicg.github.io/eyedropper-api/)
for desktop OSes using Aura.  The EyeDropper widget allows users to select any
pixel on the screen.

Win, Linux X11, and ChromeoS Ash use this widget.

Mac uses a system NSColorSampler.

Linux Wayland is not supported since Wayland does not have support to detect the
cursor position or to place the EyeDropper widget outside the browser window.

ChromeOS LaCrOS works around the Wayland restriction by using crosapi for Ash to
display this widget.
