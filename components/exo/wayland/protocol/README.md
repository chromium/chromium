This directory contains exo-specific extensions to the Wayland protocol.

To begin with, we recommend this
[link](https://wayland-book.com/xdg-shell-basics/xdg-surface.html) for more
about wayland basics. The short summary is that:
* wl_surface is the compositing window primitive. It is capable of receiving a
  series of buffers representing contents. It only provides basic functionality.
  Other functionality like pip/decorations are implemented through extensions.
* It is possible to extend a wl_surface as xdg_surface via
  xdg_wm_base.get_surface. This extension is not permanent: it is possible to
  destroy and/or recreate the xdg_surface.
* Once an xdg_surface is created, it can be assigned a role: xdg_toplevel or
  xdg_popup.
* exo has extensions for each of these primitives that implement aura shell
  (ash) specific functionalities:
  * wl_surface is extended by zaura_surface
  * xdg_toplevel is extended by zaura_toplevel
  * xdg_popup is extended by zaura_popup
