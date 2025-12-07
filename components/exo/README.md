Exo implements a display server on top of the Aura Shell. It uses the
[Wayland protocol](https://wayland.freedesktop.org/docs/html/)
to communicate with clients. For a general introduction to Wayland see
https://wayland-book.com/.

Current clients of Exo include:

* ARC++ (Android apps on Chrome OS)
* Crostini (Linux apps on Chrome OS)
* PluginVM

In addition to the core Wayland protocol, Exo supports a number of protocol
extensions. Some are third-party; see
[//third_party/wayland-protocols/README.chromium](https://chromium.googlesource.com/chromium/src/+/main/third_party/wayland-protocols/README.chromium).
Others are Chromium-specific.

A few noteworthy extensions (this list is not at all exhaustive):

* zaura_shell
  * A Chromium-specific protocol used by all Exo clients. See
    [//components/exo/wayland/protocol/aura-shell.xml](wayland/protocol/aura-shell.xml)
    and [//components/exo/wayland/zaura_shell.h](wayland/zaura_shell.h)
* zcr_remote_shell
  * A Chromium-specific protocol used exclusively by ARC++. See
    [//components/exo/wayland/zcr_remote_shell.h](wayland/zcr_remote_shell.h) and
    [//components/exo/client_controlled_shell_surface.h](client_controlled_shell_surface.h)
* zwp_fullscreen_shell
  * A third-party protocol, used in Chromium only by Chromecast. See
    [//components/exo/wayland/zwp_fullscreen_shell.h](wayland/zwp_fullscreen_shell.h)
