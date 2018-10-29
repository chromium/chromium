var globalVar = 2011;
onconnect = function(e) {
  var port = e.ports[0];
  console.log('connected');
  port.postMessage("pong");
}

