// Proxy script which sends requests to a non-existent proxy on
// success, otherwise sends them DIRECT.
let kFailure = "DIRECT";
let kSuccess = "PROXY 255.255.255.255:1";

// Returns true if |ip| is a valid IP literal.
function isIpAddress(ip) {
  // This relies on isInNetEx() parsing the IP literal, and /0 trivially matching
  // all IPs. (Since validating an IPv6 literal is not trivial)
  return isInNetEx(ip, "0.0.0.0/0") || isInNetEx(ip, "::/0");
}

function isLoopback(ip) {
  return isInNetEx(ip, "127.0.0.1/8") || isInNetEx(ip, "::1/128");
}

// Verifies that |s| is a valid result from myIpAddress() or myIpAddressEx().
function isValidResult(s) {
  if (!s)
    return false;

  let ips = s.split(";");

  if (ips.length == 0)
    return false;

  for (let ip of ips) {
    if (isLoopback(ip) || !isIpAddress(ip))
      return false;
  }

  return true;
}

function FindProxyForURL(url, host) {
  let r1 = myIpAddress();
  if (!isValidResult(r1)) {
    alert("myIpAddress() unexpectedly returned: " + JSON.stringify(r1));
    return kFailure;
  }

  let r2 = myIpAddressEx();
  if (!isValidResult(r2)) {
    alert("myIpAddressEx() unexpectedly returned: " + JSON.stringify(r2));
    return kFailure;
  }

  return kSuccess;
}
