# Firewall Hole Component Security Model

## Overview
`//chromeos/components/firewall_hole` provides the chromeos-chrome interface
(`chromeos::FirewallHole`) for requesting firewall port access via the ChromeOS
`PermissionBroker` D-Bus service.

## Process Boundaries and Isolation
1. **Renderer Isolation:**
   Renderer processes operate within a multi-layered sandbox (User/PID/Mount
   namespaces and Seccomp-BPF filters) that completely isolates them from the
   system D-Bus daemon (`/run/dbus/system_bus_socket`). Sockets, connections,
   and host filesystem operations are denied (`EPERM`). Renderer processes
   cannot directly issue D-Bus calls.

2. **Browser Brokering:**
   Trusted browser process (running as user `chronos`) interacts with
   `PermissionBroker` over the system D-Bus. When web applications or
   extensions (such as Isolated Web Apps using Direct Sockets, Crostini, or
   Nearby Share) request network capabilities, the browser process mediates and
   enforces origin checks, user permissions, and enterprise policies (e.g.,
   `CrostiniPortForwardingAllowed`, `DefaultDirectSocketsSetting`) before
   requesting a firewall hole. On the OS side, chronos and root users are
   able to call this endpoint as well - this enables tast testing.

3. **Lifeline File Descriptor Mechanism:**
   Every firewall hole request is tied to an anonymous pipe lifeline
   descriptor. The `permission_broker` system daemon monitors this descriptor.
   If the client process terminates or closes the descriptor,
   `permission_broker` automatically cleans up and removes the corresponding
   kernel firewall rule.

4. **D-Bus Policy Scope:**
   The D-Bus policy for `org.chromium.PermissionBroker` is defined in ChromeOS
   platform configuration
   (`platform2/permission_broker/org.chromium.PermissionBroker.conf`). Allowing
   the `chronos` user to make these requests is required for legitimate browser
   network features.
